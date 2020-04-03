// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_locker.h"

#include "base/lazy_instance.h"

namespace base {

constexpr size_t kLockPoolSize = 32;

class LockPool {
 public:
  LockPool() = default;
  ~LockPool() = default;

  Lock* GetLock() {
    base::AutoLock lock(lock_);
    if (index_ == kLockPoolSize)
      index_ = 0;
    return &pool_[index_];
  }

 private:
  base::Lock lock_;
  Lock pool_[kLockPoolSize];
  size_t index_ = kLockPoolSize;

  DISALLOW_COPY_AND_ASSIGN(LockPool);
};

base::LazyInstance<LockPool>::Leaky g_lock_pool =
    LAZY_INSTANCE_INITIALIZER;

Lock* GetGlobalLock() {
  return g_lock_pool.Get().GetLock();
}

SharedMemoryLocker::SharedMemoryLocker() = default;

SharedMemoryLocker::~SharedMemoryLocker() = default;

SharedMemoryLocker* SharedMemoryLocker::GetInstance() {
  static SharedMemoryLocker* instance = new SharedMemoryLocker;
  return instance;
}

void SharedMemoryLocker::LockGuid(const UnguessableToken& guid) {
  scoped_refptr<GuidLocker> locker = nullptr;
  {
    AutoLock lock(guid_lock_);
    auto it = guid_locks_.find(guid);
    if (it != guid_locks_.end()) {
      locker = it->second;
      it->second->AddRef();
    } else {
      locker = new GuidLocker(guid, GetGlobalLock());
      guid_locks_.emplace(guid, locker);
    }
  }
  locker->lock_->Acquire();
}

void SharedMemoryLocker::UnlockGuid(const UnguessableToken& guid) {
  AutoLock lock(guid_lock_);
  auto it = guid_locks_.find(guid);
  CHECK(it != guid_locks_.end());
  it->second->lock_->Release();

  if (it->second->HasOneRef())
    guid_locks_.erase(it);
  else
    it->second->Release();
}

SharedMemoryLocker::GuidLocker::GuidLocker(const UnguessableToken& guid, Lock* lock)
  : guid_(guid),
    lock_(lock) {}

SharedMemoryLocker::GuidLocker::~GuidLocker() = default;

}  // namespace base
