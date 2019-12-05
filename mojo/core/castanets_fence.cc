// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/castanets_fence.h"

namespace mojo {
namespace core {

scoped_refptr<CastanetsFence> CastanetsFence::Create(
    const base::UnguessableToken& guid,
    FenceId fence_id) {
  return new CastanetsFence(guid, fence_id);
}

void CastanetsFence::Wait() {
  if (!event_.IsSignaled())
    event_.Wait();
}

CastanetsFenceManager::CastanetsFenceManager() = default;

CastanetsFenceManager::~CastanetsFenceManager() {
  CHECK(fence_map_.empty());
}

void CastanetsFenceManager::FenceAdded(
    scoped_refptr<CastanetsFence> added_fence) {
  base::AutoLock lock(map_lock_);
  auto it = fence_map_.find(added_fence->guid_);
  if (it == fence_map_.end()) {
    fence_map_.emplace(
        std::piecewise_construct, std::forward_as_tuple(added_fence->guid_),
        std::forward_as_tuple(
            std::initializer_list<scoped_refptr<CastanetsFence>>{added_fence}));
  } else {
    it->second.push(added_fence);
  }
}

void CastanetsFenceManager::FenceRemoved(
    scoped_refptr<CastanetsFence> removed_fence) {
  base::AutoLock lock(map_lock_);
  auto it = fence_map_.find(removed_fence->guid_);
  CHECK(it != fence_map_.end());
  FenceQueue& fence_queue = it->second;
  fence_queue.pop();
  if (fence_queue.empty())
    fence_map_.erase(it);
}

base::Optional<FenceQueue> CastanetsFenceManager::GetFences(
    const base::UnguessableToken& guid) {
  base::AutoLock lock(map_lock_);
  auto it = fence_map_.find(guid);
  if (it != fence_map_.end()) {
    FenceQueue fence_queue(it->second);
    return std::move(fence_queue);
  }
  return base::nullopt;
}

CastanetsFenceQueue::CastanetsFenceQueue(CastanetsFenceManager* manager)
    : manager_(manager) {
  CHECK(manager_);
}

CastanetsFenceQueue::~CastanetsFenceQueue() {
  CHECK(fence_queue_.empty());
  CHECK(complete_queue_.empty());
}

void CastanetsFenceQueue::AddFence(const base::UnguessableToken& guid,
                                   FenceId fence_id) {
  base::AutoLock lock(lock_);
  if (complete_queue_.empty()) {
    scoped_refptr<CastanetsFence> new_fence =
        CastanetsFence::Create(guid, fence_id);
    fence_queue_.push(new_fence);
    manager_->FenceAdded(new_fence);
    return;
  }

  FenceId complete_id = complete_queue_.front();
  CHECK(fence_id == complete_id);
  complete_queue_.pop();
}

void CastanetsFenceQueue::RemoveFence(const base::UnguessableToken& guid,
                                      FenceId fence_id) {
  base::AutoLock lock(lock_);
  if (fence_queue_.empty()) {
    complete_queue_.push(fence_id);
    return;
  }

  scoped_refptr<CastanetsFence> fence = fence_queue_.front();
  CHECK(guid == fence->guid_);
  CHECK(fence_id == fence->fence_id_);
  fence->event_.Signal();
  fence_queue_.pop();
  manager_->FenceRemoved(fence);
}

}  // namespace core
}  // namespace mojo
