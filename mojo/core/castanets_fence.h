// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CASTANETS_FENCE_H_
#define MOJO_CORE_CASTANETS_FENCE_H_

#include <map>
#include <set>

#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/unguessable_token.h"

namespace mojo {
namespace core {

typedef uint32_t FenceId;

// Each BUFFER_SYNC message is paired with one Fence
class CastanetsFence : public base::RefCountedThreadSafe<CastanetsFence> {
 public:
  static scoped_refptr<CastanetsFence> Create(
      const base::UnguessableToken& guid,
      FenceId fence_id);

  void Wait();

 private:
  friend class base::RefCountedThreadSafe<CastanetsFence>;
  friend class CastanetsFenceManager;
  friend class CastanetsFenceQueue;

  CastanetsFence(const base::UnguessableToken& guid, FenceId fence_id)
      : guid_(guid),
        fence_id_(fence_id),
        event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~CastanetsFence() = default;

  base::UnguessableToken guid_;
  FenceId fence_id_;
  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(CastanetsFence);
};

// A FenceQueue notify the FenceManager whenever a Fence is pushed or poped.
// The manager manages FenceQueues based on GUID.
typedef std::queue<scoped_refptr<CastanetsFence>> FenceQueue;

class CastanetsFenceManager {
 public:
  CastanetsFenceManager();
  ~CastanetsFenceManager();

  void FenceAdded(scoped_refptr<CastanetsFence> added_fence);
  void FenceRemoved(scoped_refptr<CastanetsFence> removed_fence);

  base::Optional<FenceQueue> GetFences(const base::UnguessableToken& guid);

 private:
  base::Lock map_lock_;
  std::map<base::UnguessableToken, FenceQueue> fence_map_;

  DISALLOW_COPY_AND_ASSIGN(CastanetsFenceManager);
};

// Each node has one FenceQueue. When you receive a BUFFER_SYNC message,
// create a Fence and push it. When you're done syncing, pop the Fence
// to remove it.
class CastanetsFenceQueue {
 public:
  CastanetsFenceQueue() = delete;
  CastanetsFenceQueue(CastanetsFenceManager* manager);
  ~CastanetsFenceQueue();

  void AddFence(const base::UnguessableToken& guid, FenceId fence_id);
  void RemoveFence(const base::UnguessableToken& guid, FenceId fence_id);

 private:
  base::Lock lock_;
  FenceQueue fence_queue_;
  base::queue<FenceId> complete_queue_;

  CastanetsFenceManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(CastanetsFenceQueue);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CASTANETS_FENCE_H_
