// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_helper.h"

#if defined(OS_CHROMEOS)
#include <sys/resource.h>
#include <sys/time.h>

#include "base/debug/alias.h"
#endif  // defined(OS_CHROMEOS)

#include "base/threading/thread_restrictions.h"

#if defined(CASTANETS)
#include "base/memory/shared_memory_tracker.h"
#if defined(OS_ANDROID)
#include <sys/mman.h>
#include "third_party/ashmem/ashmem.h"
#endif
#if defined(OS_WIN)
#include <aclapi.h>
#include <stddef.h>
#include <stdint.h>
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#endif
#endif  // defined(CASTANETS)

namespace base {
#if !defined(OS_WIN)
struct ScopedPathUnlinkerTraits {
  static const FilePath* InvalidValue() { return nullptr; }

  static void Free(const FilePath* path) {
    if (unlink(path->value().c_str()))
      PLOG(WARNING) << "unlink";
  }
};

// Unlinks the FilePath when the object is destroyed.
using ScopedPathUnlinker =
    ScopedGeneric<const FilePath*, ScopedPathUnlinkerTraits>;
#endif

#if !defined(OS_ANDROID)
#if defined(OS_WIN)

HANDLE CreateFileMappingWithReducedPermissions(SECURITY_ATTRIBUTES* sa,
                                               size_t rounded_size,
                                               LPCWSTR name) {
  HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE, 0,
                               static_cast<DWORD>(rounded_size), name);
  if (!h) {
    // LogError(CREATE_FILE_MAPPING_FAILURE, GetLastError());
    return nullptr;
  }

  HANDLE dup_handle;
  BOOL success = ::DuplicateHandle(
      GetCurrentProcess(), h, GetCurrentProcess(), &dup_handle,
      FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY, FALSE, 0);
  BOOL rv = ::CloseHandle(h);
  DCHECK(rv);

  if (!success) {
    LOG(ERROR)<<" Failure ";
    return nullptr;
  }
  return dup_handle;
}

bool CreateAnonymousSharedMemory(const SharedMemoryCreateOptions& options,
                                 win::ScopedHandle* handle) {
  // TODO(crbug.com/210609): NaCl forces us to round up 64k here, wasting 32k
  // per mapping on average.
  static const size_t kSectionMask = 65536 - 1;
  DCHECK(!options.executable);
  if (options.size == 0) {
    LOG(ERROR) << " Failure ";
    return false;
  }

  // Check maximum accounting for overflow.
  if (options.size >
      static_cast<size_t>(std::numeric_limits<int>::max()) - kSectionMask) {
    LOG(ERROR) << " Failure ";
    return false;
  }

  size_t rounded_size = (options.size + kSectionMask) & ~kSectionMask;
  string16 name_ =
      options.name_deprecated ? ASCIIToUTF16(*options.name_deprecated) : L"";
  SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, FALSE};
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  if (name_.empty()) {
    // Add an empty DACL to enforce anonymous read-only sections.
    sa.lpSecurityDescriptor = &sd;
    if (!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) {
      LOG(ERROR) << " Failure ";
      return false;
    }
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
      LOG(ERROR) << " Failure ";
      return false;
    }
    if (!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE)) {
      LOG(ERROR) << " Failure ";
      return false;
    }

    // Windows ignores DACLs on certain unnamed objects (like shared sections).
    // So, we generate a random name when we need to enforce read-only.
    uint64_t rand_values[4];
    RandBytes(&rand_values, sizeof(rand_values));
    name_ = StringPrintf(L"CrSharedMem_%016llx%016llx%016llx%016llx",
                         rand_values[0], rand_values[1], rand_values[2],
                         rand_values[3]);
  }
  DCHECK(!name_.empty());
  HANDLE h =
      CreateFileMappingWithReducedPermissions(&sa, rounded_size, name_.c_str());
  if (h == nullptr) {
    // The error is logged within CreateFileMappingWithReducedPermissions().
    LOG(ERROR) << " Failure ";
    return false;
  }
  handle->Set(h);

  return true;
}
#else
bool CreateAnonymousSharedMemory(const SharedMemoryCreateOptions& options,
                                 ScopedFD* fd,
                                 ScopedFD* readonly_fd,
                                 FilePath* path) {
#if defined(OS_LINUX)
  // It doesn't make sense to have a open-existing private piece of shmem
  DCHECK(!options.open_existing_deprecated);
#endif  // defined(OS_LINUX)
  // Q: Why not use the shm_open() etc. APIs?
  // A: Because they're limited to 4mb on OS X.  FFFFFFFUUUUUUUUUUU
  FilePath directory;
  ScopedPathUnlinker path_unlinker;
  if (!GetShmemTempDir(options.executable, &directory))
    return false;

  fd->reset(base::CreateAndOpenFdForTemporaryFileInDir(directory, path));

  if (!fd->is_valid())
    return false;

  // Deleting the file prevents anyone else from mapping it in (making it
  // private), and prevents the need for cleanup (once the last fd is
  // closed, it is truly freed).
  path_unlinker.reset(path);

  if (options.share_read_only) {
    // Also open as readonly so that we can GetReadOnlyHandle.
    readonly_fd->reset(HANDLE_EINTR(open(path->value().c_str(), O_RDONLY)));
    if (!readonly_fd->is_valid()) {
      DPLOG(ERROR) << "open(\"" << path->value() << "\", O_RDONLY) failed";
      fd->reset();
      return false;
    }
  }
  return true;
}

bool PrepareMapFile(ScopedFD fd,
                    ScopedFD readonly_fd,
                    int* mapped_file,
                    int* readonly_mapped_file) {
  DCHECK_EQ(-1, *mapped_file);
  DCHECK_EQ(-1, *readonly_mapped_file);
  if (!fd.is_valid())
    return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  if (readonly_fd.is_valid()) {
    struct stat st = {};
    if (fstat(fd.get(), &st))
      NOTREACHED();

    struct stat readonly_st = {};
    if (fstat(readonly_fd.get(), &readonly_st))
      NOTREACHED();
    if (st.st_dev != readonly_st.st_dev || st.st_ino != readonly_st.st_ino) {
      LOG(ERROR) << "writable and read-only inodes don't match; bailing";
      return false;
    }
  }

  *mapped_file = HANDLE_EINTR(dup(fd.get()));
  if (*mapped_file == -1) {
    NOTREACHED() << "Call to dup failed, errno=" << errno;

#if defined(OS_CHROMEOS)
    if (errno == EMFILE) {
      // We're out of file descriptors and are probably about to crash somewhere
      // else in Chrome anyway. Let's collect what FD information we can and
      // crash.
      // Added for debugging crbug.com/733718
      int original_fd_limit = 16384;
      struct rlimit rlim;
      if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        original_fd_limit = rlim.rlim_cur;
        if (rlim.rlim_max > rlim.rlim_cur) {
          // Increase fd limit so breakpad has a chance to write a minidump.
          rlim.rlim_cur = rlim.rlim_max;
          if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            PLOG(ERROR) << "setrlimit() failed";
          }
        }
      } else {
        PLOG(ERROR) << "getrlimit() failed";
      }

      const char kFileDataMarker[] = "FDATA";
      char buf[PATH_MAX];
      char fd_path[PATH_MAX];
      char crash_buffer[32 * 1024] = {0};
      char* crash_ptr = crash_buffer;
      base::debug::Alias(crash_buffer);

      // Put a marker at the start of our data so we can confirm where it
      // begins.
      crash_ptr = strncpy(crash_ptr, kFileDataMarker, strlen(kFileDataMarker));
      for (int i = original_fd_limit; i >= 0; --i) {
        memset(buf, 0, arraysize(buf));
        memset(fd_path, 0, arraysize(fd_path));
        snprintf(fd_path, arraysize(fd_path) - 1, "/proc/self/fd/%d", i);
        ssize_t count = readlink(fd_path, buf, arraysize(buf) - 1);
        if (count < 0) {
          PLOG(ERROR) << "readlink failed for: " << fd_path;
          continue;
        }

        if (crash_ptr + count + 1 < crash_buffer + arraysize(crash_buffer)) {
          crash_ptr = strncpy(crash_ptr, buf, count + 1);
        }
        LOG(ERROR) << i << ": " << buf;
      }
      LOG(FATAL) << "Logged for file descriptor exhaustion, crashing now";
    }
#endif  // defined(OS_CHROMEOS)
  }
  *readonly_mapped_file = readonly_fd.release();

  return true;
}
#endif
#elif defined(OS_ANDROID) && defined(CASTANETS)
bool CreateAnonymousSharedMemory(const SharedMemoryCreateOptions& options,
                                 ScopedFD* fd,
                                 ScopedFD* readonly_fd,
                                 FilePath* path) {
  // "name" is just a label in ashmem. It is visible in /proc/pid/maps.
  fd->reset(ashmem_create_region(
      options.name_deprecated ? options.name_deprecated->c_str() : "",
      options.size));

  int flags = PROT_READ | PROT_WRITE | (options.executable ? PROT_EXEC : 0);
  int err = ashmem_set_prot_region(fd->get(), flags);
  if (err < 0) {
    DLOG(ERROR) << "Error " << err << " when setting protection of ashmem";
    return false;
  }

  readonly_fd->reset(dup(fd->get()));
  return true;
}
#endif  // !defined(OS_ANDROID)

#if defined(CASTANETS)
subtle::PlatformSharedMemoryRegion CreateAnonymousSharedMemoryIfNeeded(
    const UnguessableToken& guid,
    const SharedMemoryCreateOptions& option) {
  // This function theoretically can block on the disk. Both profiling of real
  // users and local instrumentation shows that this is a real problem.
  // https://code.google.com/p/chromium/issues/detail?id=466437
  ThreadRestrictions::ScopedAllowIO allow_io;
  static base::Lock* lock = new base::Lock;
  base::AutoLock auto_lock(*lock);

  subtle::PlatformSharedMemoryRegion region =
      SharedMemoryTracker::GetInstance()->FindMemoryHolder(guid);
  if (region.IsValid())
    return region;

#if defined(OS_WIN)
  win::ScopedHandle handle;
  if (!CreateAnonymousSharedMemory(option, &handle))
    return subtle::PlatformSharedMemoryRegion();

  subtle::PlatformSharedMemoryRegion::Mode mode =
      subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
  if (option.share_read_only) {
    mode = subtle::PlatformSharedMemoryRegion::Mode::kReadOnly;
  }

  region = subtle::PlatformSharedMemoryRegion::Take(std::move(handle), mode,
                                                    option.size, guid);

  SharedMemoryTracker::GetInstance()->AddHolder(region.Duplicate());
  return region;
#else

  ScopedFD new_fd;
  ScopedFD readonly_fd;
  FilePath path;
  VLOG(1) << "Create anonymous shared memory for Castanets" << guid;
  if (!CreateAnonymousSharedMemory(option, &new_fd, &readonly_fd, &path))
    return subtle::PlatformSharedMemoryRegion();
#if !defined(OS_ANDROID)
  struct stat stat;
  CHECK(!fstat(new_fd.get(), &stat));
  const size_t current_size = stat.st_size;
  if (current_size != option.size)
    CHECK(!HANDLE_EINTR(ftruncate(new_fd.get(), option.size)));
#endif

  subtle::PlatformSharedMemoryRegion::Mode mode =
      subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
  if (option.share_read_only) {
    mode = subtle::PlatformSharedMemoryRegion::Mode::kReadOnly;
    if (!readonly_fd.is_valid())
      readonly_fd.reset(HANDLE_EINTR(dup(new_fd.get())));
  }

  region = subtle::PlatformSharedMemoryRegion::Take(
      subtle::ScopedFDPair(std::move(new_fd), std::move(readonly_fd)), mode,
      option.size, guid);

  SharedMemoryTracker::GetInstance()->AddHolder(region.Duplicate());
  return region;
#endif
}
#endif  // defined(CASTANETS)

}  // namespace base
