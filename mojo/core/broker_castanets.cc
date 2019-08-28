// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/broker_castanets.h"

#include <fcntl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/castanets_memory_mapping.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_helper.h"
#include "base/memory/shared_memory_locker.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/core/broker_messages.h"
#include "mojo/core/channel.h"
#include "mojo/core/connection_params.h"
#include "mojo/core/platform_handle_utils.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/platform/tcp_platform_handle_utils.h"

namespace mojo {
namespace core {

namespace {

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

  incoming_handles->reserve(incoming_fds.size());
  for (size_t i = 0; i < incoming_fds.size(); ++i)
    incoming_handles->emplace_back(std::move(incoming_fds[i]));

  return message;
}

}  // namespace

BrokerCastanets::BrokerCastanets(PlatformHandle handle,
                                 scoped_refptr<base::TaskRunner> io_task_runner)
    : host_(false),
      sync_channel_(std::move(handle)) {
  CHECK(sync_channel_.is_valid());
  io_thread_checker_.DetachFromThread();

  int fd = sync_channel_.GetFD().get();
  // Mark the channel as blocking.
  int flags = fcntl(fd, F_GETFL);
  PCHECK(flags != -1);
  flags = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
  PCHECK(flags != -1);

  // Wait for the first message, which should contain a handle.
  std::vector<PlatformHandle> incoming_platform_handles;
  Channel::MessagePtr message =
      WaitForBrokerMessage(fd, BrokerMessageType::INIT, 1, sizeof(InitData),
                           &incoming_platform_handles);
  if (incoming_platform_handles.size() > 0 &&
      incoming_platform_handles[0].is_valid()) {
    // Received the fd for node channel with Unix Domain Socket.
    inviter_endpoint_ =
        PlatformChannelEndpoint(std::move(incoming_platform_handles[0]));
    LOG(INFO) << "Connection Success: Unix Domain Socket";
  } else {
    tcp_connection_ = true;
    // Received the port number of tcp server socket for node channel.
    const BrokerMessageHeader* header =
        static_cast<const BrokerMessageHeader*>(message->payload());
    const InitData* data = reinterpret_cast<const InitData*>(header + 1);
    inviter_endpoint_ = PlatformChannelEndpoint(
        PlatformHandle(CreateTCPClientHandle(data->port)));
    io_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&BrokerCastanets::StartChannelOnIOThread,
                                  base::Unretained(this)));
    LOG(INFO) << "Connection Success: TCP/IP Socket -> IPC Port: " << data->port;
  }
}

void BrokerCastanets::StartChannelOnIOThread() {
  CHECK(io_thread_checker_.CalledOnValidThread());
  channel_ = Channel::Create(
      this,
      ConnectionParams(PlatformChannelEndpoint(
          PlatformHandle(base::ScopedFD(sync_channel_.GetFD().get())))),
      base::ThreadTaskRunnerHandle::Get());
  channel_->Start();
}

BrokerCastanets::BrokerCastanets(base::ProcessHandle client_process,
                                 ConnectionParams connection_params,
                                 const ProcessErrorCallback& process_error_callback)
    : process_error_callback_(process_error_callback),
      host_(true)
#if defined(OS_WIN)
      ,
      client_process_(ScopedProcessHandle::CloneFrom(client_process))
#endif
{
  CHECK(connection_params.endpoint().is_valid() ||
        connection_params.server_endpoint().is_valid());
  CHECK(io_thread_checker_.CalledOnValidThread());

  sync_channel_ = PlatformHandle(base::ScopedFD(
      connection_params.server_endpoint().platform_handle().GetFD().get()));

  channel_ = Channel::Create(this, std::move(connection_params),
                             base::ThreadTaskRunnerHandle::Get());
  channel_->Start();
}

BrokerCastanets::~BrokerCastanets() {
  if (channel_)
    channel_->ShutDown();
}

void BrokerCastanets::SendSyncEvent(
    scoped_refptr<base::CastanetsMemoryMapping> mapping_info,
    size_t offset,
    size_t sync_size) {
  CHECK(tcp_connection_);
  SyncSharedBufferImpl(mapping_info->guid(),
                       static_cast<uint8_t*>(mapping_info->GetMemory()), offset,
                       sync_size, mapping_info->mapped_size());
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

void BrokerCastanets::SyncSharedBufferImpl(const base::UnguessableToken& guid,
                                           uint8_t* memory, size_t offset,
                                           size_t sync_size, size_t mapped_size) {
  bool in_io_thread = io_thread_checker_.CalledOnValidThread(); // workaround
  if (!in_io_thread)
    BeginSync(guid);

  CHECK_GE(mapped_size, offset + sync_size);
  BufferSyncData* buffer_sync = nullptr;
  void* extra_data = nullptr;
  Channel::MessagePtr out_message =
      CreateBrokerMessage(BrokerMessageType::BUFFER_SYNC, 0, sync_size,
                          &buffer_sync, &extra_data);

  buffer_sync->guid_high = guid.GetHighForSerialization();
  buffer_sync->guid_low = guid.GetLowForSerialization();
  buffer_sync->offset = offset;
  buffer_sync->sync_bytes = sync_size;
  buffer_sync->buffer_bytes = mapped_size;
  memcpy(extra_data, memory + offset, sync_size);

  VLOG(2) << "Send Sync" << guid << " offset: " << offset
          << ", sync_size: " << sync_size
          << ", buffer_size: " << buffer_sync->buffer_bytes;
  channel_->Write(std::move(out_message));

  if (!in_io_thread)
    WaitSync(guid);
}

void BrokerCastanets::OnBufferSync(uint64_t guid_high, uint64_t guid_low,
                                   uint32_t offset, uint32_t sync_bytes,
                                   uint32_t buffer_bytes, const void* data) {
  CHECK(tcp_connection_);
  base::UnguessableToken guid =
      base::UnguessableToken::Deserialize(guid_high, guid_low);

  base::AutoGuidLock guid_lock(guid);

  VLOG(2) << "Recv sync" << guid << " offset: " << offset
          << ", sync_size: " << sync_bytes
          << ", buffer_size: " << buffer_bytes;

  scoped_refptr<base::CastanetsMemoryMapping> mapping =
      base::SharedMemoryTracker::GetInstance()->FindMappedMemory(guid);
  if (mapping) {
    CHECK_GE(mapping->mapped_size(), offset + sync_bytes);
    memcpy(static_cast<uint8_t*>(mapping->GetMemory()) + offset, data,
           sync_bytes);

    SendSyncAck(guid_high, guid_low);
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

  SendSyncAck(guid_high, guid_low);
}

void BrokerCastanets::SendSyncAck(uint64_t guid_high, uint64_t guid_low) {
  BufferSyncAckData* sync_ack;
  Channel::MessagePtr out_message = CreateBrokerMessage(
      BrokerMessageType::BUFFER_SYNC_ACK, 0, 0, &sync_ack);
  sync_ack->guid_high = guid_high;
  sync_ack->guid_low = guid_low;
  channel_->Write(std::move(out_message));
}

PlatformChannelEndpoint BrokerCastanets::GetInviterEndpoint() {
  return std::move(inviter_endpoint_);
}

base::WritableSharedMemoryRegion BrokerCastanets::GetWritableSharedMemoryRegion(
    size_t num_bytes) {
  if (tcp_connection_) {
    base::subtle::PlatformSharedMemoryRegion region =
        base::subtle::PlatformSharedMemoryRegion::CreateWritable(num_bytes);
    base::WritableSharedMemoryRegion r =
        base::WritableSharedMemoryRegion::Deserialize(std::move(region));
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

#if !defined(OS_POSIX) || (defined(OS_ANDROID) && !defined(CASTANETS)) || defined(OS_FUCHSIA) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
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

bool BrokerCastanets::SendPortNumber(int port) {
  CHECK(port != -1);
  CHECK(channel_);
  tcp_connection_ = true;

  InitData* data;
  Channel::MessagePtr message =
      CreateBrokerMessage(BrokerMessageType::INIT, 0, 0, &data);
#if defined(OS_WIN)
  data->pipe_name_length = 0;
#endif
  data->port = port;

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
#if !defined(OS_POSIX) || (defined(OS_ANDROID) && !defined(CASTANETS)) || defined(OS_FUCHSIA) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
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
        OnBufferSync(sync->guid_high, sync->guid_low, sync->offset,
            sync->sync_bytes, sync->buffer_bytes, sync + 1);
      else
        LOG(WARNING) << "Wrong size for sync data";
      break;
    }

    case BrokerMessageType::BUFFER_SYNC_ACK:
      if (payload_size ==
          sizeof(BrokerMessageHeader) + sizeof(BufferSyncAckData)) {
        const BufferSyncAckData* sync_ack =
            reinterpret_cast<const BufferSyncAckData*>(header + 1);
        base::UnguessableToken guid =
            base::UnguessableToken::Deserialize(sync_ack->guid_high,
                                                sync_ack->guid_low);
        EndSync(guid);
      }
      break;

    default:
      DLOG(ERROR) << "Unexpected broker message type: " << header->type;
      break;
  }
}

void BrokerCastanets::BeginSync(const base::UnguessableToken& guid) {
  base::AutoLock lock(sync_lock_);
  CHECK(sync_waits_.find(guid) == sync_waits_.end());

  sync_waits_.emplace(std::piecewise_construct,
                      std::make_tuple(guid), std::make_tuple());
}

void BrokerCastanets::EndSync(const base::UnguessableToken& guid) {
  base::AutoLock lock(sync_lock_);
  auto it = sync_waits_.find(guid);
  if (it == sync_waits_.end())
    return;
  it->second.Signal();
}

void BrokerCastanets::WaitSync(const base::UnguessableToken& guid) {
  SyncWaitMap::iterator it;
  {
    base::AutoLock lock(sync_lock_);
    it = sync_waits_.find(guid);
    if (it == sync_waits_.end())
      return;
  }
  if (!it->second.IsSignaled())
    it->second.Wait();
  {
    base::AutoLock lock(sync_lock_);
    sync_waits_.erase(it);
  }
}

void BrokerCastanets::OnChannelError(Channel::Error error) {
  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Broker host received malformed message");
  }
}

}  // namespace core
}  // namespace mojo
