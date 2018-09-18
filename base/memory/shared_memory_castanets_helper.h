// Copyright 2018 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_CASTANETS_HELPER_H_
#define BASE_MEMORY_SHARED_MEMORY_CASTANETS_HELPER_H_

namespace base {

namespace nfs_util {

#if defined(NFS_SHARED_MEMORY)
// This method flushes the changes on memory mapped region to
// the underlying network file system.
inline void FlushToDisk(int fd) {
  fdatasync(fd);
}

// Temporary workaround to get nfs server->nfs client updates
// synched.
inline void Sync(int fd) {
  FILE* fp = fdopen(fd, "rw");
  fseek(fp, 0L, SEEK_END);
}
#endif

}  // namespace nfs_util

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_CASTANETS_HELPER_H_
