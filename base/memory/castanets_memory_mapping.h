// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_CASTANETS_MEMORY_MAPPING_H_
#define BASE_MEMORY_CASTANETS_MEMORY_MAPPING_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"

namespace base {

class BASE_EXPORT CastanetsMemoryMapping
    : public RefCountedThreadSafe<CastanetsMemoryMapping> {
public:
  static scoped_refptr<CastanetsMemoryMapping>
  Create(const UnguessableToken &id, size_t size);

  void AddMapping(void *address);
  void RemoveMapping(void *address);

  void UpdateCurrentSize(size_t size) { current_size_ += size; }
  size_t current_size() { return current_size_; }

  UnguessableToken guid() const { return guid_; }
  size_t mapped_size() const { return mapped_size_; }
  void *GetMemory();

  bool HasMapping() const { return !addresses_.empty(); }

  void *MapForSync(int fd);
  void UnmapForSync(void *memory);

private:
  friend class base::RefCountedThreadSafe<CastanetsMemoryMapping>;

  CastanetsMemoryMapping(const UnguessableToken &id, size_t size);
  ~CastanetsMemoryMapping();

  UnguessableToken guid_;
  size_t mapped_size_ = 0;
  size_t current_size_ = 0;

  std::vector<void *> addresses_;

  DISALLOW_COPY_AND_ASSIGN(CastanetsMemoryMapping);
};

} // namespace base

#endif // BASE_MEMORY_CASTANETS_MEMORY_MAPPING_H_
