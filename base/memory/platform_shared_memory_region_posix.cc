// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "base/files/file_util.h"
#include "base/numerics/checked_math.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

namespace base {
namespace subtle {

namespace {

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

#if !defined(OS_NACL)
bool CheckFDAccessMode(int fd, int expected_mode) {
  int fd_status = fcntl(fd, F_GETFL);
  if (fd_status == -1) {
    DPLOG(ERROR) << "fcntl(" << fd << ", F_GETFL) failed";
    return false;
  }

  int mode = fd_status & O_ACCMODE;
  if (mode != expected_mode) {
    DLOG(ERROR) << "Descriptor access mode (" << mode
                << ") differs from expected (" << expected_mode << ")";
    return false;
  }

  return true;
}
#endif  // !defined(OS_NACL)

}  // namespace

ScopedFDPair::ScopedFDPair() = default;

ScopedFDPair::ScopedFDPair(ScopedFDPair&&) = default;

ScopedFDPair& ScopedFDPair::operator=(ScopedFDPair&&) = default;

ScopedFDPair::~ScopedFDPair() = default;

ScopedFDPair::ScopedFDPair(ScopedFD in_fd, ScopedFD in_readonly_fd)
    : fd(std::move(in_fd)), readonly_fd(std::move(in_readonly_fd)) {}

FDPair ScopedFDPair::get() const {
  return {fd.get(), readonly_fd.get()};
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    ScopedFDPair handle,
    Mode mode,
    size_t size,
#if defined(CASTANETS)
    const UnguessableToken& guid, int sid) {
#else
    const UnguessableToken& guid) {
#endif
  if (!handle.fd.is_valid())
    return {};

  if (size == 0)
    return {};

  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

#if !defined(CASTANETS)
  CHECK(
      CheckPlatformHandlePermissionsCorrespondToMode(handle.get(), mode, size));
#endif

  switch (mode) {
    case Mode::kReadOnly:
    case Mode::kUnsafe:
      if (handle.readonly_fd.is_valid()) {
        handle.readonly_fd.reset();
        DLOG(WARNING) << "Readonly handle shouldn't be valid for a "
                         "non-writable memory region; closing";
      }
      break;
    case Mode::kWritable:
      if (!handle.readonly_fd.is_valid()) {
        DLOG(ERROR)
            << "Readonly handle must be valid for writable memory region";
        return {};
      }
      break;
    default:
      DLOG(ERROR) << "Invalid permission mode: " << static_cast<int>(mode);
      return {};
  }

#if defined(CASTANETS)
  return PlatformSharedMemoryRegion(std::move(handle), mode, size, guid, sid);
#else
  return PlatformSharedMemoryRegion(std::move(handle), mode, size, guid);
#endif
}

FDPair PlatformSharedMemoryRegion::GetPlatformHandle() const {
  return handle_.get();
}

bool PlatformSharedMemoryRegion::IsValid() const {
  return handle_.fd.is_valid() &&
         (mode_ == Mode::kWritable ? handle_.readonly_fd.is_valid() : true);
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Duplicate() const {
  if (!IsValid())
    return {};

  CHECK_NE(mode_, Mode::kWritable)
      << "Duplicating a writable shared memory region is prohibited";

  ScopedFD duped_fd(HANDLE_EINTR(dup(handle_.fd.get())));
  if (!duped_fd.is_valid()) {
    DPLOG(ERROR) << "dup(" << handle_.fd.get() << ") failed";
    return {};
  }

  return PlatformSharedMemoryRegion({std::move(duped_fd), ScopedFD()}, mode_,
#if defined(CASTANETS)
                                    size_, guid_, memory_file_id_);
#else
                                    size_, guid_);
#endif
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

  handle_.fd.reset(handle_.readonly_fd.release());
  mode_ = Mode::kReadOnly;
  return true;
}

bool PlatformSharedMemoryRegion::ConvertToUnsafe() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to unsafe";

  handle_.readonly_fd.reset();
  mode_ = Mode::kUnsafe;
  return true;
}

bool PlatformSharedMemoryRegion::MapAt(off_t offset,
                                       size_t size,
                                       void** memory,
                                       size_t* mapped_size) const {
  if (!IsValid())
    return false;

  size_t end_byte;
  if (!CheckAdd(offset, size).AssignIfValid(&end_byte) || end_byte > size_) {
    return false;
  }

  bool write_allowed = mode_ != Mode::kReadOnly;
  *memory = mmap(nullptr, size, PROT_READ | (write_allowed ? PROT_WRITE : 0),
                 MAP_SHARED, handle_.fd.get(), offset);

  bool mmap_succeeded = *memory && *memory != MAP_FAILED;
  if (!mmap_succeeded) {
    DPLOG(ERROR) << "mmap " << handle_.fd.get() << " failed";
    return false;
  }

  *mapped_size = size;
  DCHECK_EQ(0U,
            reinterpret_cast<uintptr_t>(*memory) & (kMapMinimumAlignment - 1));
  return true;
}

#if defined(CASTANETS)
bool FilePathForName(const std::string& mem_name,
                     FilePath* path) {
  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK_EQ(std::string::npos, mem_name.find('/'));
  DCHECK_EQ(std::string::npos, mem_name.find('\0'));

  FilePath temp_dir;
  if (!GetShmemTempDir(false, &temp_dir))
    return false;

#if defined(GOOGLE_CHROME_BUILD)
  static const char kShmem[] = "com.google.Chrome.shmem.";
#else
  static const char kShmem[] = ".org.chromium.Chromium.shmem.";
#endif
  CR_DEFINE_STATIC_LOCAL(const std::string, name_base, (kShmem));
  *path = temp_dir.AppendASCII(name_base + mem_name);
  return true;
}
#endif

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
#if defined(CASTANETS)
                                                              size_t size, std::string name) {
#else
                                                              size_t size) {
#endif
#if defined(OS_NACL)
  // Untrusted code can't create descriptors or handles.
  return {};
#else
  if (size == 0)
    return {};

  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  ThreadRestrictions::ScopedAllowIO allow_io;

  // We don't use shm_open() API in order to support the --disable-dev-shm-usage
  // flag.
  FilePath directory;
  if (!GetShmemTempDir(false /* executable */, &directory))
    return {};

  ScopedFD fd;
  FilePath path;
#if defined(CASTANETS)
  int memory_file_id = 0;
  if(name.empty()) {
  fd.reset(CreateAndOpenFdForTemporaryFileInDir(directory, &path, &memory_file_id));
#else
  fd.reset(CreateAndOpenFdForTemporaryFileInDir(directory, &path));
#endif

  if (!fd.is_valid()) {
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
    FilePath dir = path.DirName();
    if (access(dir.value().c_str(), W_OK | X_OK) < 0) {
      PLOG(ERROR) << "Unable to access(W_OK|X_OK) " << dir.value();
      if (dir.value() == "/dev/shm") {
        LOG(FATAL) << "This is frequently caused by incorrect permissions on "
                   << "/dev/shm.  Try 'sudo chmod 1777 /dev/shm' to fix.";
      }
    }
    return {};
  }
#if defined(CASTANETS)
  } else {
    if (!FilePathForName(name, &path))
      return {};

    // Make sure that the file is opened without any permission
    // to other users on the system.
    const mode_t kOwnerOnly = //S_IRUSR | S_IWUSR;
    S_IRUSR | S_IWUSR | S_IRWXU| S_IRWXO;
    // First, try to create the file.
    fd.reset(HANDLE_EINTR(
        open(path.value().c_str(), O_RDWR | O_CREAT | O_EXCL, kOwnerOnly)));
    if (!fd.is_valid()) {
      // If this doesn't work, try and open an existing file in append mode.
      // Opening an existing file in a world writable directory has two main
      // security implications:
      // - Attackers could plant a file under their control, so ownership of
      //   the file is checked below.
      // - Attackers could plant a symbolic link so that an unexpected file
      //   is opened, so O_NOFOLLOW is passed to open().
#if !defined(OS_AIX)
      fd.reset(HANDLE_EINTR(
          open(path.value().c_str(), O_RDWR | O_APPEND | O_NOFOLLOW)));
#else
      // AIX has no 64-bit support for open flags such as -
      //  O_CLOEXEC, O_NOFOLLOW and O_TTY_INIT.
      fd.reset(HANDLE_EINTR(open(path.value().c_str(), O_RDWR | O_APPEND)));
#endif
      // An existing file was opened, so its size should not be fixed.
    }
  }
#endif
  // Deleting the file prevents anyone else from mapping it in (making it
  // private), and prevents the need for cleanup (once the last fd is
  // closed, it is truly freed).
#if !defined(CASTANETS)
  ScopedPathUnlinker path_unlinker(&path);
#endif

  ScopedFD readonly_fd;
  if (mode == Mode::kWritable) {
    // Also open as readonly so that we can ConvertToReadOnly().
    readonly_fd.reset(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
    if (!readonly_fd.is_valid()) {
      DPLOG(ERROR) << "open(\"" << path.value() << "\", O_RDONLY) failed";
      return {};
    }
  }

  // Get current size.
  struct stat stat = {};
  if (fstat(fd.get(), &stat) != 0)
    return {};
  const size_t current_size = stat.st_size;
  if (current_size != size) {
    if (HANDLE_EINTR(ftruncate(fd.get(), size)) != 0)
      return {};
  }

  if (readonly_fd.is_valid()) {
    struct stat readonly_stat = {};
    if (fstat(readonly_fd.get(), &readonly_stat))
      NOTREACHED();

    if (stat.st_dev != readonly_stat.st_dev ||
        stat.st_ino != readonly_stat.st_ino) {
      LOG(ERROR) << "Writable and read-only inodes don't match; bailing";
      return {};
    }
  }
  return PlatformSharedMemoryRegion({std::move(fd), std::move(readonly_fd)},
#if defined(CASTANETS)
                                    mode, size, UnguessableToken::Create(),memory_file_id);
#else
                                    mode, size, UnguessableToken::Create());
#endif
#endif  // !defined(OS_NACL)
}

bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformHandle handle,
    Mode mode,
    size_t size) {
#if !defined(OS_NACL)
  if (!CheckFDAccessMode(handle.fd,
                         mode == Mode::kReadOnly ? O_RDONLY : O_RDWR)) {
    return false;
  }

  if (mode == Mode::kWritable)
    return CheckFDAccessMode(handle.readonly_fd, O_RDONLY);

  // The second descriptor must be invalid in kReadOnly and kUnsafe modes.
  if (handle.readonly_fd != -1) {
    DLOG(ERROR) << "The second descriptor must be invalid";
    return false;
  }

  return true;
#else
  // fcntl(_, F_GETFL) is not implemented on NaCl.
  void* temp_memory = nullptr;
  temp_memory =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle.fd, 0);

  bool mmap_succeeded = temp_memory && temp_memory != MAP_FAILED;
  if (mmap_succeeded)
    munmap(temp_memory, size);

  bool is_read_only = !mmap_succeeded;
  bool expected_read_only = mode == Mode::kReadOnly;

  if (is_read_only != expected_read_only) {
    DLOG(ERROR) << "Descriptor has a wrong access mode: it is"
                << (is_read_only ? " " : " not ") << "read-only but it should"
                << (expected_read_only ? " " : " not ") << "be";
    return false;
  }

  return true;
#endif  // !defined(OS_NACL)
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    ScopedFDPair handle,
    Mode mode,
    size_t size,
#if defined(CASTANETS)
    const UnguessableToken& guid, int sid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {
  memory_file_id_ = sid;
}
#else
    const UnguessableToken& guid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {}
#endif

}  // namespace subtle
}  // namespace base
