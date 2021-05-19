// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>

#include "base/memory/castanets_memory_mapping.h"
#include "base/notreached.h"

namespace base {

scoped_refptr<CastanetsMemoryMapping>
CastanetsMemoryMapping::Create(const UnguessableToken &id, size_t size) {
  return new CastanetsMemoryMapping(id, size);
}

CastanetsMemoryMapping::CastanetsMemoryMapping(const UnguessableToken &id,
                                               size_t size)
    : guid_(id), mapped_size_(size) {}

CastanetsMemoryMapping::~CastanetsMemoryMapping() { CHECK(addresses_.empty()); }

void CastanetsMemoryMapping::AddMapping(void *address) {
  addresses_.push_back(address);
}

void CastanetsMemoryMapping::RemoveMapping(void *address) {
  for (auto it = addresses_.begin(); it < addresses_.end(); ++it) {
    if (*it == address) {
      addresses_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void *CastanetsMemoryMapping::GetMemory() {
  CHECK(HasMapping());
  return *(addresses_.begin());
}

void *CastanetsMemoryMapping::MapForSync(int fd) {
  CHECK(fd != -1);
  void *memory =
      mmap(NULL, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  CHECK(memory);
  AddMapping(memory);
  return memory;
}

void CastanetsMemoryMapping::UnmapForSync(void *memory) {
  CHECK(memory);
  munmap(memory, mapped_size_);
  RemoveMapping(memory);
}

} // namespace base
