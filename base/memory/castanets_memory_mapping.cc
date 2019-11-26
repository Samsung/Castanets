// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(_WINDOWS)
#include <aclapi.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <sys/mman.h>
#endif

#include "base/memory/castanets_memory_mapping.h"

namespace base {

scoped_refptr<CastanetsMemoryMapping> CastanetsMemoryMapping::Create(
    const UnguessableToken& id,
    size_t size) {
  return new CastanetsMemoryMapping(id, size);
}

CastanetsMemoryMapping::CastanetsMemoryMapping(const UnguessableToken& id,
                                               size_t size)
    : guid_(id), mapped_size_(size) {}

CastanetsMemoryMapping::~CastanetsMemoryMapping() {
  CHECK(addresses_.empty());
}

void CastanetsMemoryMapping::AddMapping(void* address) {
  addresses_.push_back(address);
}

void CastanetsMemoryMapping::RemoveMapping(void* address) {
  for (auto it = addresses_.begin(); it < addresses_.end(); ++it) {
    if (*it == address) {
      addresses_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void* CastanetsMemoryMapping::GetMemory() {
  CHECK(HasMapping());
  return *(addresses_.begin());
}

void* CastanetsMemoryMapping::MapForSync(int fd) {
  CHECK(fd != -1);
#if defined(OS_WIN)
  void* memory =
      MapViewOfFile(HANDLE(fd), FILE_MAP_READ | FILE_MAP_WRITE,
         static_cast<uint64_t>(0) >> 32, static_cast<DWORD> (0), mapped_size_);
#else
  void* memory =
      mmap(NULL, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
#endif
  CHECK(memory);
  AddMapping(memory);
  return memory;
}

void CastanetsMemoryMapping::UnmapForSync(void* memory) {
  CHECK(memory);
  RemoveMapping(memory);
#if defined(OS_WIN)
  UnmapViewOfFile(memory);
#else
  munmap(memory, mapped_size_);
#endif
}

}  // namespace base
