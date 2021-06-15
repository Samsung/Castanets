// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_HELPER_H_
#define BASE_MEMORY_SHARED_MEMORY_HELPER_H_

#include "build/build_config.h"

#include "base/base_export.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <stdio.h>
#include <sys/types.h>
#include <semaphore.h>
#include "base/file_descriptor_posix.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif


#if defined(OS_CHROMEOS)
#include <sys/resource.h>
#include <sys/time.h>

#include "base/debug/alias.h"
#endif  // defined(OS_CHROMEOS)

#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#if defined(CASTANETS)
#include "base/memory/platform_shared_memory_region.h"
#endif // defined(CASTANETS)

#include <fcntl.h>

namespace base {

struct BASE_EXPORT SharedMemoryCreateOptions {
#if !defined(OS_FUCHSIA)
  // DEPRECATED (crbug.com/345734):
  // If NULL, the object is anonymous.  This pointer is owned by the caller
  // and must live through the call to Create().
  const std::string* name_deprecated = nullptr;

  // DEPRECATED (crbug.com/345734):
  // If true, and the shared memory already exists, Create() will open the
  // existing shared memory and ignore the size parameter.  If false,
  // shared memory must not exist.  This flag is meaningless unless
  // name_deprecated is non-NULL.
  bool open_existing_deprecated = false;
#endif

  // Size of the shared memory object to be created.
  // When opening an existing object, this has no effect.
  size_t size = 0;

  // If true, mappings might need to be made executable later.
  bool executable = false;

  // If true, the file can be shared read-only to a process.
  bool share_read_only = false;
};

#if !defined(OS_ANDROID)
// Makes a temporary file, fdopens it, and then unlinks it. |fd| is populated
// with the opened fd. |readonly_fd| is populated with the opened fd if
// options.share_read_only is true. |path| is populated with the location of
// the file before it was unlinked.
// Returns false if there's an unhandled failure.
bool CreateAnonymousSharedMemory(const SharedMemoryCreateOptions& options,
                                 ScopedFD* fd,
                                 ScopedFD* readonly_fd,
                                 FilePath* path);
#elif defined(OS_ANDROID) && defined(CASTANETS)
bool CreateAnonymousSharedMemory(const SharedMemoryCreateOptions& options,
                                 ScopedFD* fd,
                                 ScopedFD* readonly_fd,
                                 FilePath* path);
#endif // !defined(OS_ANDROID)

// Takes the outputs of CreateAnonymousSharedMemory and maps them properly to
// |mapped_file| or |readonly_mapped_file|, depending on which one is populated.
bool PrepareMapFile(ScopedFD fd,
                    ScopedFD readonly_fd,
                    int* mapped_file,
                    int* readonly_mapped_file);

#if defined(CASTANETS)
subtle::PlatformSharedMemoryRegion BASE_EXPORT
CreateAnonymousSharedMemoryIfNeeded(const UnguessableToken& guid,
                                    const SharedMemoryCreateOptions& option);
#endif // defined(CASTANETS)

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_HELPER_H_
