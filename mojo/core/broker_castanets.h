// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_CASTANETS_H_
#define MOJO_CORE_BROKER_CASTANETS_H_

#include "base/memory/castanets_memory_syncer.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/message_loop/message_loop_current.h"
#include "mojo/core/channel.h"
#include "mojo/core/embedder/process_error_callback.h"
#include "mojo/core/platform_handle_in_transit.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/sync.h"

namespace mojo {
namespace core {

class CastanetsFenceManager;
class CastanetsFenceQueue;
class NodeChannel;

// The Broker is a channel to the broker process, which allows synchronous IPCs
// to fulfill shared memory allocation requests on some platforms.
class BrokerCastanets : public Channel::Delegate, public base::SyncDelegate {
 public:
  static scoped_refptr<BrokerCastanets> CreateInBrowserProcess(
      base::ProcessHandle client_process,
      ConnectionParams connection_params,
      const ProcessErrorCallback& process_error_callback,
      CastanetsFenceManager* fence_manager);

  static scoped_refptr<BrokerCastanets> CreateInChildProcess(
      ConnectionParams connection_params,
      scoped_refptr<base::TaskRunner> io_task_runner,
      CastanetsFenceManager* fence_manager);

  void SetNodeChannel(scoped_refptr<NodeChannel> node_channel) {
    CHECK(node_channel);
    node_channel_ = node_channel;
  }
  void ResetNodeChannel(ConnectionParams connection_params,
                        ScopedProcessHandle process_handle);

  void ResetBrokerChannel(ConnectionParams connection_params);

  // Returns ConnectionParams that should be used to establish a NodeChannel
  // to the process which is inviting us to join its network.
  ConnectionParams GetInviterConnectionParams();

  // Request a shared buffer from the broker process. Blocks the current thread.
  base::WritableSharedMemoryRegion GetWritableSharedMemoryRegion(
      size_t num_bytes);

  // base::SyncDelegate:
  void SendSyncEvent(scoped_refptr<base::CastanetsMemoryMapping> mapping_info,
                     size_t offset,
                     size_t sync_size,
                     bool write_lock) override;

  bool SyncSharedBuffer(const base::UnguessableToken& guid,
                        size_t offset,
                        size_t sync_size,
                        BrokerCompressionMode compression_mode);

  bool SyncSharedBuffer(base::WritableSharedMemoryMapping& mapping,
                        size_t offset,
                        size_t sync_size);

  void OnBufferSync(uint64_t guid_high,
                    uint64_t guid_low,
                    uint32_t fence_id,
                    uint32_t offset,
                    uint32_t sync_bytes,
                    uint32_t buffer_bytes,
                    uint32_t original_size,
                    uint32_t compression_mode,
                    const void* data);

  void AddSyncFence(const base::UnguessableToken& guid, uint32_t fence_id);

  // Sync 2-dimensional memory for partial rasterization,
  bool SyncSharedBuffer2d(const base::UnguessableToken& guid,
                          size_t width,
                          size_t height,
                          size_t bytes_per_pixel,
                          size_t offset,
                          size_t stride,
                          BrokerCompressionMode compression_mode);

  void OnBufferSync2d(uint64_t guid_high,
                      uint64_t guid_low,
                      uint32_t fence_id,
                      uint32_t offset,
                      uint32_t sync_bytes,
                      uint32_t buffer_bytes,
                      uint32_t width,
                      uint32_t stride,
                      uint32_t original_size,
                      uint32_t compression_mode,
                      const void* data);

  // Send |handle| to the client, to be used to establish a NodeChannel to us.
  bool SendChannel(PlatformHandle handle);

#if defined(OS_WIN)
  // Sends a named channel to the client. Like above, but for named pipes.
  void SendNamedChannel(const base::StringPiece16& pipe_name);
#endif

  bool PrepareHandlesForClient(std::vector<PlatformHandleInTransit>* handles);

  // Channel::Delegate:
  void OnChannelMessage(const void* payload,
                        size_t payload_size,
                        std::vector<PlatformHandle> handles) override;
  void OnChannelError(Channel::Error error) override;

  bool is_tcp_connection() const { return tcp_connection_; }

  const ProcessErrorCallback process_error_callback_;

 private:
  // Note: This is blocking, and will wait for the first message over
  // the endpoint handle in |handle|.
  BrokerCastanets(ConnectionParams connection_params,
                  scoped_refptr<base::TaskRunner> io_task_runner,
                  CastanetsFenceManager* fence_manager);

  BrokerCastanets(base::ProcessHandle client_process,
                  ConnectionParams connection_params,
                  const ProcessErrorCallback& process_error_callback,
                  CastanetsFenceManager* fence_manager);

  ~BrokerCastanets() override;

  void Initialize(ConnectionParams connection_params,
                  scoped_refptr<base::TaskRunner> io_task_runner);

  void StartChannelOnIOThread(ConnectionParams connection_params,
                              uint16_t port,
                              bool secure_connection);

  // Send InitData to the client for node channel connection.
  bool SendBrokerInit(int port, bool secure_connection);

  void OnBufferRequest(uint32_t num_bytes);

  void SyncSharedBufferImpl(const base::UnguessableToken& guid,
                            uint8_t* memory,
                            size_t offset,
                            size_t sync_size,
                            size_t mapped_size,
                            BrokerCompressionMode compression_mode = BrokerCompressionMode::ZLIB,
                            bool write_lock = true);

  void SyncSharedBufferImpl2d(const base::UnguessableToken& guid,
                              uint8_t* memory,
                              size_t mapped_size,
                              size_t width,
                              size_t height,
                              size_t bytes_per_pixel,
                              size_t offset,
                              size_t stride,
                              BrokerCompressionMode compression_mode = BrokerCompressionMode::WEBP,
                              bool write_lock = true);

  bool tcp_connection_ = false;

  // Handle to the broker process, used for synchronous IPCs.
  PlatformHandle sync_channel_;

  // ConnectionParams connected to the inviter process. Received in the first
  // first message over |sync_channel_|.
  ConnectionParams inviter_connection_params_;

  scoped_refptr<Channel> channel_;

  scoped_refptr<NodeChannel> node_channel_;

  std::unique_ptr<CastanetsFenceQueue> fence_queue_;

  base::Lock sync_lock_;

#if defined(OS_WIN)
  ScopedProcessHandle client_process_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrokerCastanets);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_BROKER_CASTANETS_H_
