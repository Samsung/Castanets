// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker_castanets.h"

#include <fcntl.h>
#include <sys/mman.h>

#include "base/lazy_instance.h"
#include "base/memory/castanets_memory_mapping.h"
#include "base/memory/shared_memory_helper.h"
#include "base/memory/shared_memory_locker.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "crypto/random.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/castanets_fence.h"
#include "mojo/core/node_channel.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/platform/tcp_platform_handle_utils.h"

namespace mojo {
namespace core {

namespace {

constexpr size_t kRandomIdCacheSize = 256;

// This class is cloned form RandomNameGenerator in node.cc.
// Random fence id generator which maintains a cache of random bytes to draw
// from. This amortizes the cost of random id generation on platforms where
// RandBytes may have significant per-call overhead.

class RandomIdGenerator {
 public:
  RandomIdGenerator() = default;
  ~RandomIdGenerator() = default;

  FenceId GenerateRandomId() {
    base::AutoLock lock(lock_);
    if (cache_index_ == kRandomIdCacheSize) {
      crypto::RandBytes(cache_, sizeof(FenceId) * kRandomIdCacheSize);
      cache_index_ = 0;
    }
    return cache_[cache_index_++];
  }

 private:
  base::Lock lock_;
  FenceId cache_[kRandomIdCacheSize];
  size_t cache_index_ = kRandomIdCacheSize;

  DISALLOW_COPY_AND_ASSIGN(RandomIdGenerator);
};

base::LazyInstance<RandomIdGenerator>::Leaky g_id_generator =
    LAZY_INSTANCE_INITIALIZER;

FenceId GenerateRandomFenceId() {
  return g_id_generator.Get().GenerateRandomId();
}

Channel::MessagePtr WaitForBrokerMessage(
    int socket_fd,
    BrokerMessageType expected_type,
    size_t expected_num_handles,
    size_t expected_data_size,
    std::vector<PlatformHandle>* incoming_handles) {
  Channel::MessagePtr message(new Channel::Message(
      sizeof(BrokerMessageHeader) + expected_data_size, expected_num_handles));
  std::vector<base::ScopedFD> incoming_fds;
  ssize_t read_result =
      SocketRecvmsg(socket_fd, const_cast<void*>(message->data()),
                    message->data_num_bytes(), &incoming_fds, true /* block */);

  if (incoming_fds.size() != expected_num_handles) {
    incoming_fds.reserve(expected_num_handles);
    for (size_t i = 0; i < expected_num_handles; ++i)
      incoming_fds.emplace_back(base::ScopedFD(kCastanetsHandle));
  }

  bool error = false;
  if (read_result < 0) {
    PLOG(ERROR) << "Recvmsg error";
    error = true;
  } else if (static_cast<size_t>(read_result) != message->data_num_bytes()) {
    LOG(ERROR) << "Invalid node channel message";
    error = true;
  } else if (incoming_fds.size() != expected_num_handles) {
    LOG(ERROR) << "Received unexpected number of handles";
    error = true;
  }

  if (error)
    return nullptr;

  const BrokerMessageHeader* header =
      reinterpret_cast<const BrokerMessageHeader*>(message->payload());
  if (header->type != expected_type) {
    LOG(ERROR) << "Unexpected message - expected_type(" << expected_type
               << ") != header->type(" << header->type << ")";
    return nullptr;
  }

  if (incoming_handles) {
    incoming_handles->reserve(incoming_fds.size());
    for (size_t i = 0; i < incoming_fds.size(); ++i)
      incoming_handles->emplace_back(std::move(incoming_fds[i]));
  }

  return message;
}

}  // namespace

BrokerCastanets::BrokerCastanets(ConnectionParams connection_params,
                                 scoped_refptr<base::TaskRunner> io_task_runner,
                                 CastanetsFenceManager* fence_manager)
    : fence_queue_(std::make_unique<CastanetsFenceQueue>(fence_manager)) {
  Initialize(std::move(connection_params), io_task_runner);
}

void BrokerCastanets::StartChannelOnIOThread(ConnectionParams connection_params,
                                             uint16_t port,
                                             bool secure_connection) {
  const bool server_endpoint = connection_params.server_endpoint().is_valid();

  // Do not apply secure connection for Broker Channel.
  connection_params.SetSecure(false);
  channel_ = Channel::Create(this, std::move(connection_params),
                             base::ThreadTaskRunnerHandle::Get());
  channel_->Start();

  if (server_endpoint) {
    // Send Init message
    LOG(INFO) << "Send INIT message. port:" << port
              << ", ssl:" << secure_connection;
    SendBrokerInit(port, secure_connection);
  }
}

BrokerCastanets::BrokerCastanets(
    base::ProcessHandle client_process,
    ConnectionParams connection_params,
    const ProcessErrorCallback& process_error_callback,
    CastanetsFenceManager* fence_manager)
    : process_error_callback_(process_error_callback),
      fence_queue_(std::make_unique<CastanetsFenceQueue>(fence_manager))
#if defined(OS_WIN)
      ,
      client_process_(ScopedProcessHandle::CloneFrom(client_process))
#endif
{
  Initialize(std::move(connection_params), nullptr);
}

void BrokerCastanets::Initialize(
    ConnectionParams connection_params,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  CHECK(connection_params.endpoint().is_valid() ||
        connection_params.server_endpoint().is_valid());
  if (IsNetworkSocket(
          connection_params.server_endpoint().platform_handle().GetFD()) ||
      IsNetworkSocket(connection_params.endpoint().platform_handle().GetFD())) {
    tcp_connection_ = true;
    uint16_t port = 0;
    bool secure_connection = false;
    if (connection_params.server_endpoint().is_valid()) {
      // TCP Server
      secure_connection = connection_params.is_secure();
      inviter_connection_params_ =
          ConnectionParams(mojo::PlatformChannelServerEndpoint(
              mojo::CreateTCPServerHandle(port, &port)));
      if (secure_connection)
        inviter_connection_params_.SetSecure();
      // Will send INIT message in StartChannelOnIOThread
    } else {
      // TCP Client
      CHECK(!connection_params.tcp_address().empty());
      // Connect Broker Channel
      LOG(INFO) << "Connecting broker channel to "
                << connection_params.tcp_address() << ":"
                << connection_params.tcp_port();
      if (!mojo::TCPClientConnect(
              connection_params.endpoint().platform_handle().GetFD(),
              connection_params.tcp_address(), connection_params.tcp_port()))
        return;

      // Wait for the first message.
      int fd = connection_params.endpoint().platform_handle().GetFD().get();
      // Mark the channel as blocking.
      int flags = fcntl(fd, F_GETFL);
      PCHECK(flags != -1);
      flags = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
      PCHECK(flags != -1);

      LOG(INFO) << "Wait for INIT message... from fd:" << fd;
      // Wait for the first message.
      Channel::MessagePtr message = WaitForBrokerMessage(
          fd, BrokerMessageType::INIT, 0, sizeof(InitData), nullptr);
      // Received the port number of tcp server socket for node channel.
      const BrokerMessageHeader* header =
          static_cast<const BrokerMessageHeader*>(message->payload());
      const InitData* data = reinterpret_cast<const InitData*>(header + 1);
      LOG(INFO) << "Received INIT message. port:" << data->port
                << ", ssl:" << data->secure_connection;
      port = data->port;
      secure_connection = data->secure_connection;

      inviter_connection_params_ =
          ConnectionParams(mojo::PlatformChannelEndpoint(
              CreateTCPClientHandle(port, connection_params.tcp_address())));
      if (secure_connection)
        inviter_connection_params_.SetSecure();
    }
    if (io_task_runner.get()) {
      io_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&BrokerCastanets::StartChannelOnIOThread,
                         base::Unretained(this), std::move(connection_params),
                         port, secure_connection));
    } else {
      StartChannelOnIOThread(std::move(connection_params), port,
                             secure_connection);
    }
  } else {
    // Unix Domain Socket
    CHECK((connection_params.endpoint().is_valid()));
    if (io_task_runner.get()) {
      // Use |sync_channel_| only for IPC Broker without Channel.
      sync_channel_ = connection_params.TakeEndpoint().TakePlatformHandle();

      int fd = sync_channel_.GetFD().get();
      // Mark the channel as blocking.
      int flags = fcntl(fd, F_GETFL);
      PCHECK(flags != -1);
      flags = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
      PCHECK(flags != -1);

      // Wait for Init message in Child Process
      std::vector<PlatformHandle> incoming_platform_handles;
      if (WaitForBrokerMessage(fd, BrokerMessageType::INIT, 1, sizeof(InitData),
                               &incoming_platform_handles)) {
        inviter_connection_params_ =
            ConnectionParams(mojo::PlatformChannelEndpoint(
                std::move(incoming_platform_handles[0])));
        LOG(INFO) << "Connection Success: Unix Domain Socket";
      }
    } else {
      StartChannelOnIOThread(std::move(connection_params), 0, false);
    }
  }
}

BrokerCastanets::~BrokerCastanets() {
  if (channel_)
    channel_->ShutDown();
}

void BrokerCastanets::SendSyncEvent(
    scoped_refptr<base::CastanetsMemoryMapping> mapping_info,
    size_t offset,
    size_t sync_size,
    bool write_lock) {
  CHECK(tcp_connection_);
  SyncSharedBufferImpl(mapping_info->guid(),
                       static_cast<uint8_t*>(mapping_info->GetMemory()), offset,
                       sync_size, mapping_info->mapped_size(), write_lock);
}

bool BrokerCastanets::SyncSharedBuffer(
    const base::UnguessableToken& guid,
    size_t offset,
    size_t sync_size) {
  if (!tcp_connection_)
    return true;

  scoped_refptr<base::CastanetsMemoryMapping> mapping =
      base::SharedMemoryTracker::GetInstance()->FindMappedMemory(guid);
  if (!mapping)
      return MOJO_RESULT_NOT_FOUND;

  SyncSharedBufferImpl(guid, static_cast<uint8_t*>(mapping->GetMemory()),
                       offset, sync_size, mapping->mapped_size());
  return true;
}

bool BrokerCastanets::SyncSharedBuffer(
    base::WritableSharedMemoryMapping& mapping,
    size_t offset,
    size_t sync_size) {
  if (!tcp_connection_)
    return true;

  SyncSharedBufferImpl(mapping.guid(), static_cast<uint8_t*>(mapping.memory()),
                       offset, sync_size, mapping.mapped_size());
  return true;
}

bool BrokerCastanets::SyncSharedBuffer2d(const base::UnguessableToken& guid,
                                         size_t offset,
                                         size_t sync_size,
                                         size_t width,
                                         size_t stride) {
  if (!tcp_connection_)
    return true;

  scoped_refptr<base::CastanetsMemoryMapping> mapping =
      base::SharedMemoryTracker::GetInstance()->FindMappedMemory(guid);
  if (!mapping)
    return MOJO_RESULT_NOT_FOUND;

  SyncSharedBufferImpl2d(guid, static_cast<uint8_t*>(mapping->GetMemory()),
                         offset, sync_size, mapping->mapped_size(), width,
                         stride);
  return true;
}

void BrokerCastanets::SyncSharedBufferImpl(const base::UnguessableToken& guid,
                                           uint8_t* memory,
                                           size_t offset,
                                           size_t sync_size,
                                           size_t mapped_size,
                                           bool write_lock) {
  CHECK_GE(mapped_size, offset + sync_size);
  BufferSyncData* buffer_sync = nullptr;
  void* extra_data = nullptr;
  Channel::MessagePtr out_message =
      CreateBrokerMessage(BrokerMessageType::BUFFER_SYNC, 0, sync_size,
                          &buffer_sync, &extra_data);

  FenceId fence_id = GenerateRandomFenceId();
  buffer_sync->guid_high = guid.GetHighForSerialization();
  buffer_sync->guid_low = guid.GetLowForSerialization();
  buffer_sync->fence_id = fence_id;
  buffer_sync->offset = offset;
  buffer_sync->sync_bytes = sync_size;
  buffer_sync->buffer_bytes = mapped_size;
  memcpy(extra_data, memory + offset, sync_size);

  base::AutoLock lock(sync_lock_);

  VLOG(2) << "Send Sync" << guid << " offset: " << offset
          << ", sync_size: " << sync_size
          << ", buffer_size: " << buffer_sync->buffer_bytes
          << ", fence_id: " << fence_id;

  channel_->Write(std::move(out_message));
  node_channel_->AddSyncFence(guid, fence_id, write_lock);
}

void BrokerCastanets::SyncSharedBufferImpl2d(const base::UnguessableToken& guid,
                                             uint8_t* memory,
                                             size_t offset,
                                             size_t sync_size,
                                             size_t mapped_size,
                                             size_t width,
                                             size_t stride,
                                             bool write_lock) {
  CHECK_GE(mapped_size, offset + sync_size);
  BufferSyncData* buffer_sync = nullptr;
  void* extra_data = nullptr;
  Channel::MessagePtr out_message =
      CreateBrokerMessage(BrokerMessageType::BUFFER_SYNC_2D, 0, sync_size,
                          &buffer_sync, &extra_data);

  FenceId fence_id = GenerateRandomFenceId();
  buffer_sync->guid_high = guid.GetHighForSerialization();
  buffer_sync->guid_low = guid.GetLowForSerialization();
  buffer_sync->fence_id = fence_id;
  buffer_sync->offset = offset;
  buffer_sync->sync_bytes = sync_size;
  buffer_sync->buffer_bytes = mapped_size;
  buffer_sync->width = width;
  buffer_sync->stride = stride;

  size_t put_bytes = 0;
  size_t put_offset = offset;

  while (put_bytes != sync_size) {
    memcpy(static_cast<uint8_t*>(extra_data) + put_bytes, memory + put_offset,
           width);
    put_offset += stride;
    put_bytes += width;
  }

  base::AutoLock lock(sync_lock_);

  VLOG(2) << "Send Sync" << guid << " offset: " << offset
          << ", sync_size: " << sync_size
          << ", buffer_size: " << buffer_sync->buffer_bytes
          << ", width: " << buffer_sync->width
          << ", stride: " << buffer_sync->stride
          << ", fence_id: " << fence_id;

  channel_->Write(std::move(out_message));
  node_channel_->AddSyncFence(guid, fence_id, write_lock);
}

void BrokerCastanets::OnBufferSync(uint64_t guid_high,
                                   uint64_t guid_low,
                                   uint32_t fence_id,
                                   uint32_t offset,
                                   uint32_t sync_bytes,
                                   uint32_t buffer_bytes,
                                   const void* data) {
  CHECK(tcp_connection_);
  base::UnguessableToken guid =
      base::UnguessableToken::Deserialize(guid_high, guid_low);

  base::AutoGuidLock guid_lock(guid);

  VLOG(2) << "Recv sync" << guid << " offset: " << offset
          << ", sync_size: " << sync_bytes
          << ", buffer_size: " << buffer_bytes
          << ", fence_id: " << fence_id;

  scoped_refptr<base::CastanetsMemoryMapping> mapping =
      base::SharedMemoryTracker::GetInstance()->FindMappedMemory(guid);
  if (mapping) {
    CHECK_GE(mapping->mapped_size(), offset + sync_bytes);
    memcpy(static_cast<uint8_t*>(mapping->GetMemory()) + offset, data,
           sync_bytes);

    fence_queue_->RemoveFence(guid, fence_id);
    return;
  }

  base::SharedMemoryCreateOptions options;
  options.size = buffer_bytes;
  base::subtle::PlatformSharedMemoryRegion handle =
      base::CreateAnonymousSharedMemoryIfNeeded(guid, options);
  CHECK(handle.IsValid());

  void* memory = mmap(NULL, sync_bytes + offset, PROT_READ | PROT_WRITE,
                      MAP_SHARED, handle.GetPlatformHandle().fd, 0);
  uint8_t* ptr = static_cast<uint8_t*>(memory);
  memcpy(ptr + offset, data, sync_bytes);
  munmap(ptr, sync_bytes + offset);

  fence_queue_->RemoveFence(guid, fence_id);
}

void BrokerCastanets::OnBufferSync2d(uint64_t guid_high,
                                     uint64_t guid_low,
                                     uint32_t fence_id,
                                     uint32_t offset,
                                     uint32_t sync_bytes,
                                     uint32_t buffer_bytes,
                                     uint32_t width,
                                     uint32_t stride,
                                     const void* data) {
  CHECK(tcp_connection_);
  base::UnguessableToken guid =
      base::UnguessableToken::Deserialize(guid_high, guid_low);

  base::AutoGuidLock guid_lock(guid);

  VLOG(2) << "Recv sync" << guid << " offset: " << offset
          << ", sync_size: " << sync_bytes << ", buffer_size: " << buffer_bytes
          << ", width: " << width << ", stride: " << stride
          << ", fence_id: " << fence_id;

  scoped_refptr<base::CastanetsMemoryMapping> mapping =
      base::SharedMemoryTracker::GetInstance()->FindMappedMemory(guid);

  CHECK_GE(mapping->mapped_size(), offset + sync_bytes);
  size_t put_bytes = 0;
  size_t put_offset = offset;
  while (put_bytes != sync_bytes) {
    memcpy(static_cast<uint8_t*>(mapping->GetMemory()) + put_offset,
           static_cast<const uint8_t*>(data) + put_bytes, width);
    put_offset += stride;
    put_bytes += width;
  }

  fence_queue_->RemoveFence(guid, fence_id);
}

void BrokerCastanets::AddSyncFence(const base::UnguessableToken& guid,
                                   uint32_t fence_id) {
  fence_queue_->AddFence(guid, fence_id);
}

void BrokerCastanets::ResetNodeChannel(ConnectionParams node_connection_pararms,
                                       ScopedProcessHandle process_handle) {
  node_channel_->SetSocket(std::move(node_connection_pararms));
  node_channel_->Start();
  node_channel_->SetRemoteProcessHandle(std::move(process_handle));
}

void BrokerCastanets::ResetBrokerChannel(ConnectionParams connection_params) {
  tcp_connection_ = false;
  sync_channel_.reset();
  channel_->ClearOutgoingMessages();
  channel_->SetSocket(std::move(connection_params));
  channel_->Start();
}

ConnectionParams BrokerCastanets::GetInviterConnectionParams() {
  return std::move(inviter_connection_params_);
}

base::WritableSharedMemoryRegion BrokerCastanets::GetWritableSharedMemoryRegion(
    size_t num_bytes) {
  if (tcp_connection_) {
    base::subtle::PlatformSharedMemoryRegion region =
        base::subtle::PlatformSharedMemoryRegion::CreateWritable(num_bytes);
    base::WritableSharedMemoryRegion r =
        base::WritableSharedMemoryRegion::Deserialize(std::move(region));

    BufferResponseData* buffer_guid;
    Channel::MessagePtr out_message = CreateBrokerMessage(
        BrokerMessageType::BUFFER_CREATED, 0, 0, &buffer_guid);
    buffer_guid->guid_high = r.GetGUID().GetHighForSerialization();
    buffer_guid->guid_low = r.GetGUID().GetLowForSerialization();
    channel_->Write(std::move(out_message));
    return r;
  }

  BufferRequestData* buffer_request;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_REQUEST, 0, 0, &buffer_request);
  buffer_request->size = num_bytes;
  ssize_t write_result =
      SocketWrite(sync_channel_.GetFD().get(), out_message->data(),
                  out_message->data_num_bytes());
  if (write_result < 0) {
    PLOG(ERROR) << "Error sending sync broker message";
    return base::WritableSharedMemoryRegion();
  } else if (static_cast<size_t>(write_result) !=
             out_message->data_num_bytes()) {
    LOG(ERROR) << "Error sending complete broker message";
    return base::WritableSharedMemoryRegion();
  }

#if !defined(OS_POSIX) || (defined(OS_ANDROID) && !defined(CASTANETS)) || \
    defined(OS_FUCHSIA) || (defined(OS_MACOSX) && !defined(OS_IOS))
  // Non-POSIX systems, as well as Android, Fuchsia, and non-iOS Mac, only use
  // a single handle to represent a writable region.
  constexpr size_t kNumExpectedHandles = 1;
#else
  constexpr size_t kNumExpectedHandles = 2;
#endif

  std::vector<PlatformHandle> handles;
  Channel::MessagePtr message = WaitForBrokerMessage(
      sync_channel_.GetFD().get(), BrokerMessageType::BUFFER_RESPONSE,
      kNumExpectedHandles, sizeof(BufferResponseData), &handles);
  if (message) {
    const BufferResponseData* data;
    if (!GetBrokerMessageData(message.get(), &data))
      return base::WritableSharedMemoryRegion();

    if (handles.size() == 1)
      handles.emplace_back();

    return base::WritableSharedMemoryRegion::Deserialize(
        base::subtle::PlatformSharedMemoryRegion::Take(
            CreateSharedMemoryRegionHandleFromPlatformHandles(
                std::move(handles[0]), std::move(handles[1])),
            base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
            num_bytes,
            base::UnguessableToken::Deserialize(data->guid_high,
                                                data->guid_low)));
  }
  return base::WritableSharedMemoryRegion();
}

bool BrokerCastanets::SendChannel(PlatformHandle handle) {
  CHECK(handle.is_valid());
  CHECK(channel_);

#if defined(OS_WIN)
  InitData* data;
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, 0, &data);
  data->pipe_name_length = 0;
#else
  InitData* data;
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 1, 0, &data);
  data->port = -1;
  data->secure_connection = 0;
#endif
  std::vector<PlatformHandleInTransit> handles(1);
  handles[0] = PlatformHandleInTransit(std::move(handle));

  // This may legitimately fail on Windows if the client process is in another
  // session, e.g., is an elevated process.
  if (!PrepareHandlesForClient(&handles))
    return false;

  message->SetHandles(std::move(handles));
  channel_->Write(std::move(message));
  return true;
}

bool BrokerCastanets::SendBrokerInit(int port, bool secure_connection) {
  CHECK(port != -1);
  CHECK(channel_);

  InitData* data;
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 0, 0, &data);
#if defined(OS_WIN)
  data->pipe_name_length = 0;
#endif
  data->port = port;
  data->secure_connection = secure_connection ? 1 : 0;

  channel_->Write(std::move(message));
  return true;
}

#if defined(OS_WIN)

void BrokerCastanets::SendNamedChannel(const base::StringPiece16& pipe_name) {
  InitData* data;
  base::char16* name_data;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::INIT, 0, sizeof(*name_data) * pipe_name.length(),
      &data, reinterpret_cast<void**>(&name_data));
  data->pipe_name_length = static_cast<uint32_t>(pipe_name.length());
  std::copy(pipe_name.begin(), pipe_name.end(), name_data);
  channel_->Write(std::move(message));
}

#endif  // defined(OS_WIN)

bool BrokerCastanets::PrepareHandlesForClient(
    std::vector<PlatformHandleInTransit>* handles) {
#if defined(OS_WIN)
  bool handles_ok = true;
  for (auto& handle : *handles) {
    if (!handle.TransferToProcess(client_process_.Clone()))
      handles_ok = false;
  }
  return handles_ok;
#else
  return true;
#endif
}

void BrokerCastanets::OnBufferRequest(uint32_t num_bytes) {
  base::subtle::PlatformSharedMemoryRegion region =
      base::subtle::PlatformSharedMemoryRegion::CreateWritable(num_bytes);

  std::vector<PlatformHandleInTransit> handles(2);
  if (region.IsValid()) {
    PlatformHandle h[2];
    ExtractPlatformHandlesFromSharedMemoryRegionHandle(
        region.PassPlatformHandle(), &h[0], &h[1]);
    handles[0] = PlatformHandleInTransit(std::move(h[0]));
    handles[1] = PlatformHandleInTransit(std::move(h[1]));
#if !defined(OS_POSIX) || (defined(OS_ANDROID) && !defined(CASTANETS)) || \
    defined(OS_FUCHSIA) || (defined(OS_MACOSX) && !defined(OS_IOS))
    // Non-POSIX systems, as well as Android, Fuchsia, and non-iOS Mac, only use
    // a single handle to represent a writable region.
    DCHECK(!handles[1].handle().is_valid());
    handles.resize(1);
#else
    DCHECK(handles[1].handle().is_valid());
#endif
  }

  BufferResponseData* response;
  Channel::MessagePtr message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_RESPONSE, handles.size(), 0, &response);
  if (!handles.empty()) {
    base::UnguessableToken guid = region.GetGUID();
    response->guid_high = guid.GetHighForSerialization();
    response->guid_low = guid.GetLowForSerialization();
    PrepareHandlesForClient(&handles);
    message->SetHandles(std::move(handles));
  }

  channel_->Write(std::move(message));
}

void BrokerCastanets::OnChannelMessage(const void* payload,
                                       size_t payload_size,
                                       std::vector<PlatformHandle> handles) {
  if (payload_size < sizeof(BrokerMessageHeader))
    return;

  const BrokerMessageHeader* header =
      static_cast<const BrokerMessageHeader*>(payload);
  switch (header->type) {
    case BrokerMessageType::BUFFER_REQUEST:
      if (payload_size ==
          sizeof(BrokerMessageHeader) + sizeof(BufferRequestData)) {
        const BufferRequestData* request =
            reinterpret_cast<const BufferRequestData*>(header + 1);
        OnBufferRequest(request->size);
      }
      break;

    case BrokerMessageType::BUFFER_SYNC:
    {
      const BufferSyncData* sync =
          reinterpret_cast<const BufferSyncData*>(header + 1);
      if (payload_size == sizeof(BrokerMessageHeader) +
          sizeof(BufferSyncData) + sync->sync_bytes)
        OnBufferSync(sync->guid_high, sync->guid_low, sync->fence_id,
                     sync->offset, sync->sync_bytes, sync->buffer_bytes,
                     sync + 1);
      else
        LOG(WARNING) << "Wrong size for sync data";
      break;
    }

    case BrokerMessageType::BUFFER_SYNC_2D: {
      const BufferSyncData* sync =
          reinterpret_cast<const BufferSyncData*>(header + 1);
      if (payload_size == sizeof(BrokerMessageHeader) + sizeof(BufferSyncData) +
                              sync->sync_bytes)
        OnBufferSync2d(sync->guid_high, sync->guid_low, sync->fence_id,
                       sync->offset, sync->sync_bytes, sync->buffer_bytes,
                       sync->width, sync->stride, sync + 1);
      else
        LOG(WARNING) << "Wrong size for sync data";
      break;
    }

    case BrokerMessageType::BUFFER_CREATED: {
      const BufferResponseData* buffer =
          reinterpret_cast<const BufferResponseData*>(header + 1);
      if (payload_size == sizeof(BrokerMessageHeader) +
                          sizeof(BufferResponseData)) {
        base::UnguessableToken guid =
            base::UnguessableToken::Deserialize(buffer->guid_high,
                                                buffer->guid_low);
        base::SharedMemoryTracker::GetInstance()->OnBufferCreated(guid, this);
      } else
        LOG(WARNING) << "Wrong size for sync data";
      break;
    }

    default:
      DLOG(ERROR) << "Unexpected broker message type: " << header->type;
      break;
  }
}

void BrokerCastanets::OnChannelError(Channel::Error error) {
  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Broker host received malformed message");
  }
}

scoped_refptr<BrokerCastanets> BrokerCastanets::CreateInBrowserProcess(
    base::ProcessHandle client_process,
    ConnectionParams connection_params,
    const ProcessErrorCallback& process_error_callback,
    CastanetsFenceManager* fence_manager) {
  return new BrokerCastanets(client_process, std::move(connection_params),
                             process_error_callback, fence_manager);
}

scoped_refptr<BrokerCastanets> BrokerCastanets::CreateInChildProcess(
    ConnectionParams connection_params,
    scoped_refptr<base::TaskRunner> io_task_runner,
    CastanetsFenceManager* fence_manager) {
  return new BrokerCastanets(std::move(connection_params), io_task_runner,
                             fence_manager);
}

}  // namespace core
}  // namespace mojo
