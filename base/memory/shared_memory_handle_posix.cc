// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_handle.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/unguessable_token.h"

#if defined(CASTANETS)
#include "base/distributed_chromium_util.h"
#endif

namespace base {

SharedMemoryHandle::SharedMemoryHandle() = default;

SharedMemoryHandle::SharedMemoryHandle(
    const base::FileDescriptor& file_descriptor,
    size_t size,
#if !defined(NETWORK_SHARED_MEMORY)
    const base::UnguessableToken& guid)
#else
    const base::UnguessableToken& guid, int shared_memory_file_id)
#endif
    : file_descriptor_(file_descriptor), guid_(guid), size_(size) {
#if defined(NETWORK_SHARED_MEMORY)
  shared_memory_file_id_ = shared_memory_file_id;
#endif
}

// static
#if !defined(NETWORK_SHARED_MEMORY)
SharedMemoryHandle SharedMemoryHandle::ImportHandle(int fd, size_t size) {
#else
SharedMemoryHandle SharedMemoryHandle::ImportHandle(int fd, size_t size, int shared_memory_file_id) {
#endif
  SharedMemoryHandle handle;
  handle.file_descriptor_.fd = fd;
  handle.file_descriptor_.auto_close = false;
  handle.guid_ = UnguessableToken::Create();
  handle.size_ = size;
#if defined(NETWORK_SHARED_MEMORY)
  handle.shared_memory_file_id_ = shared_memory_file_id;
#endif
  return handle;
}

int SharedMemoryHandle::GetHandle() const {
  return file_descriptor_.fd;
}

bool SharedMemoryHandle::IsValid() const {
  return file_descriptor_.fd >= 0;
}

void SharedMemoryHandle::Close() const {
#if defined(CASTANETS)
  if (base::Castanets::IsEnabled() &&
          file_descriptor_.fd == 0) {
    return;
  }
#endif

  if (IGNORE_EINTR(close(file_descriptor_.fd)) < 0)
    PLOG(ERROR) << "close";
}

int SharedMemoryHandle::Release() {
  int old_fd = file_descriptor_.fd;
  file_descriptor_.fd = -1;
  return old_fd;
}

SharedMemoryHandle SharedMemoryHandle::Duplicate() const {
  if (!IsValid())
    return SharedMemoryHandle();

#if defined(CASTANETS)
  if (base::Castanets::IsEnabled() &&
          file_descriptor_.fd == 0) {
#if defined(NETWORK_SHARED_MEMORY)
    return SharedMemoryHandle(FileDescriptor(0, true), GetSize(), GetGUID(), GetMemoryFileId());
#else
    return SharedMemoryHandle(FileDescriptor(0, true), GetSize(), GetGUID());
#endif
  }
#endif

  int duped_handle = HANDLE_EINTR(dup(file_descriptor_.fd));
  if (duped_handle < 0)
    return SharedMemoryHandle();
  return SharedMemoryHandle(FileDescriptor(duped_handle, true), GetSize(),
#if defined(NETWORK_SHARED_MEMORY)
                            GetGUID(), GetMemoryFileId());
#else
                            GetGUID());
#endif
}

void SharedMemoryHandle::SetOwnershipPassesToIPC(bool ownership_passes) {
  file_descriptor_.auto_close = ownership_passes;
}

bool SharedMemoryHandle::OwnershipPassesToIPC() const {
  return file_descriptor_.auto_close;
}

}  // namespace base
