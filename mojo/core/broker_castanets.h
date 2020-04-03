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

namespace mojo {
namespace core {

class CastanetsFenceManager;
class CastanetsFenceQueue;
class NodeChannel;

// The Broker is a channel to the broker process, which allows synchronous IPCs
// to fulfill shared memory allocation requests on some platforms.
class BrokerCastanets : public Channel::Delegate, public base::SyncDelegate {
 public:
  // Note: This is blocking, and will wait for the first message over
  // the endpoint handle in |handle|.
  explicit BrokerCastanets(PlatformHandle handle,
                           scoped_refptr<base::TaskRunner> io_task_runner,
                           CastanetsFenceManager* fence_manager);

  ~BrokerCastanets() override;

  void SetNodeChannel(scoped_refptr<NodeChannel> node_channel) {
    CHECK(node_channel);
    node_channel_ = node_channel;
  }

  // Returns the platform handle that should be used to establish a NodeChannel
  // to the process which is inviting us to join its network. This is the first
  // handle read off the Broker channel upon construction.
  PlatformChannelEndpoint GetInviterEndpoint();

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
                        size_t sync_size);

  bool SyncSharedBuffer(base::WritableSharedMemoryMapping& mapping,
                        size_t offset,
                        size_t sync_size);

  void OnBufferSync(uint64_t guid_high,
                    uint64_t guid_low,
                    uint32_t fence_id,
                    uint32_t offset,
                    uint32_t sync_bytes,
                    uint32_t buffer_bytes,
                    const void* data);

  void AddSyncFence(const base::UnguessableToken& guid, uint32_t fence_id);

  BrokerCastanets(base::ProcessHandle client_process,
             ConnectionParams connection_params,
             const ProcessErrorCallback& process_error_callback,
             CastanetsFenceManager* fence_manager);

  // Send a port number to the client for TCP socket.
  bool SendPortNumber(int port);

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

  const ProcessErrorCallback process_error_callback_;

 private:
  void StartChannelOnIOThread();

  void OnBufferRequest(uint32_t num_bytes);

  void SyncSharedBufferImpl(const base::UnguessableToken& guid,
                            uint8_t* memory,
                            size_t offset,
                            size_t sync_size,
                            size_t mapped_size,
                            bool write_lock = true);

  bool tcp_connection_ = false;

  // Handle to the broker process, used for synchronous IPCs.
  PlatformHandle sync_channel_;

  // Channel endpoint connected to the inviter process. Recieved in the first
  // first message over |sync_channel_|.
  PlatformChannelEndpoint inviter_endpoint_;

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
