// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_LOCKER_H_
#define BASE_MEMORY_SHARED_MEMORY_LOCKER_H_

#include <map>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"

namespace base {

// SharedMemoryLocker provides the lock for the corresponding guid.
class BASE_EXPORT SharedMemoryLocker {
 public:
  // Returns a singleton instance.
  static SharedMemoryLocker* GetInstance();

  void LockGuid(const UnguessableToken& guid);
  void UnlockGuid(const UnguessableToken& guid);

 private:
  SharedMemoryLocker();
  ~SharedMemoryLocker();

  class GuidLocker : public RefCountedThreadSafe<GuidLocker> {
   private:
    friend class RefCountedThreadSafe<GuidLocker>;
    friend class SharedMemoryLocker;

    GuidLocker(const UnguessableToken& guid, Lock* lock);
    ~GuidLocker();

    UnguessableToken guid_;
    Lock* const lock_;
  };

  Lock guid_lock_;
  std::map<UnguessableToken, scoped_refptr<GuidLocker>> guid_locks_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryLocker);
};

class AutoGuidLock {
 public:
  explicit AutoGuidLock(const UnguessableToken& guid)
    : guid_(guid) {
    SharedMemoryLocker::GetInstance()->LockGuid(guid_);
  }

  ~AutoGuidLock() {
    SharedMemoryLocker::GetInstance()->UnlockGuid(guid_);
  }

 private:
  UnguessableToken guid_;
  DISALLOW_COPY_AND_ASSIGN(AutoGuidLock);
};

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_LOCKER_H_
