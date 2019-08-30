// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/castanets_memory_syncer.h"
#include "base/memory/castanets_memory_mapping.h"

namespace base {

UnknownMemorySyncer::UnknownMemorySyncer(
    scoped_refptr<CastanetsMemoryMapping> mapping_info)
    : mapping_info_(mapping_info), fd_in_transit_(-1) {}

UnknownMemorySyncer::UnknownMemorySyncer(int fd) : fd_in_transit_(fd) {}

UnknownMemorySyncer::UnknownMemorySyncer(
    scoped_refptr<CastanetsMemoryMapping> mapping_info,
    int fd)
    : mapping_info_(mapping_info), fd_in_transit_(fd) {}

UnknownMemorySyncer::~UnknownMemorySyncer() = default;

UnguessableToken UnknownMemorySyncer::GetGUID() const {
  return mapping_info_->guid();
}

void UnknownMemorySyncer::SetMappingInfo(
    scoped_refptr<CastanetsMemoryMapping> mapping_info) {
  CHECK(!mapping_info_);
  mapping_info_ = mapping_info;
}

void UnknownMemorySyncer::SetFdInTransit(int fd) {
  if (fd_in_transit_ != -1)
    pending_syncs_.clear();

  fd_in_transit_ = fd;
}

std::unique_ptr<ExternalMemorySyncer> UnknownMemorySyncer::ConvertToExternal(
    SyncDelegate* delegate) {
  if (!pending_syncs_.empty() && mapping_info_) {
    void* memory = nullptr;
    if (!mapping_info_->HasMapping())
      memory = mapping_info_->MapForSync(fd_in_transit_);

    for (auto& it : pending_syncs_)
      delegate->SendSyncEvent(mapping_info_, it.offset, it.size, false);

    if (memory)
      mapping_info_->UnmapForSync(memory);
    pending_syncs_.clear();
  }

  if (!mapping_info_ || !mapping_info_->HasMapping())
    return nullptr;

  return std::make_unique<ExternalMemorySyncer>(delegate, mapping_info_);
}

void UnknownMemorySyncer::SyncMemory(size_t offset, size_t sync_size) {
  pending_syncs_.push_back(SyncInfo{offset, sync_size});
}

ExternalMemorySyncer::ExternalMemorySyncer(
    SyncDelegate* delegate,
    scoped_refptr<CastanetsMemoryMapping> mapping)
    : delegate_(delegate), mapping_info_(mapping) {}

ExternalMemorySyncer::~ExternalMemorySyncer() = default;

void ExternalMemorySyncer::SyncMemory(size_t offset, size_t sync_size) {
  delegate_->SendSyncEvent(mapping_info_, offset, sync_size);
}

}  // namespace base
