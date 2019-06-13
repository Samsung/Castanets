// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_tracker.h"

#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

#if defined(CASTANETS)
#include "base/memory/platform_shared_memory_region.h"
#endif // defined(CASTANETS)

namespace base {

const char SharedMemoryTracker::kDumpRootName[] = "shared_memory";

// static
SharedMemoryTracker* SharedMemoryTracker::GetInstance() {
  static SharedMemoryTracker* instance = new SharedMemoryTracker;
  return instance;
}

// static
std::string SharedMemoryTracker::GetDumpNameForTracing(
    const UnguessableToken& id) {
  DCHECK(!id.is_empty());
  return std::string(kDumpRootName) + "/" + id.ToString();
}

// static
trace_event::MemoryAllocatorDumpGuid
SharedMemoryTracker::GetGlobalDumpIdForTracing(const UnguessableToken& id) {
  std::string dump_name = GetDumpNameForTracing(id);
  return trace_event::MemoryAllocatorDumpGuid(dump_name);
}

// static
const trace_event::MemoryAllocatorDump*
SharedMemoryTracker::GetOrCreateSharedMemoryDump(
    const SharedMemory* shared_memory,
    trace_event::ProcessMemoryDump* pmd) {
  return GetOrCreateSharedMemoryDumpInternal(shared_memory->memory(),
                                             shared_memory->mapped_size(),
                                             shared_memory->mapped_id(), pmd);
}

const trace_event::MemoryAllocatorDump*
SharedMemoryTracker::GetOrCreateSharedMemoryDump(
    const SharedMemoryMapping& shared_memory,
    trace_event::ProcessMemoryDump* pmd) {
  return GetOrCreateSharedMemoryDumpInternal(shared_memory.raw_memory_ptr(),
                                             shared_memory.mapped_size(),
                                             shared_memory.guid(), pmd);
}

void SharedMemoryTracker::IncrementMemoryUsage(
    const SharedMemory& shared_memory) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(shared_memory.memory()) == usages_.end());
  usages_.emplace(shared_memory.memory(), UsageInfo(shared_memory.mapped_size(),
                                                    shared_memory.mapped_id()));
#if defined(CASTANETS)
  const UnguessableToken& guid = shared_memory.mapped_id();
  auto it = mappings_.find(guid);
  if (it == mappings_.end()) {
    mappings_[guid] = &shared_memory;
    VLOG(2) << "Add mapping" << guid << " num: " << mappings_.size();
  }
#endif
}

void SharedMemoryTracker::IncrementMemoryUsage(
    const SharedMemoryMapping& mapping) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(mapping.raw_memory_ptr()) == usages_.end());
  usages_.emplace(mapping.raw_memory_ptr(),
                  UsageInfo(mapping.mapped_size(), mapping.guid()));
}

void SharedMemoryTracker::DecrementMemoryUsage(
    const SharedMemory& shared_memory) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(shared_memory.memory()) != usages_.end());
  usages_.erase(shared_memory.memory());

#if defined(CASTANETS)
  auto it = mappings_.find(shared_memory.mapped_id());
  if (it != mappings_.end()) {
    mappings_.erase(it);
    VLOG(2) << "Del mapping" << shared_memory.mapped_id()
            << " num: " << mappings_.size();
  }
#endif
}

void SharedMemoryTracker::DecrementMemoryUsage(
    const SharedMemoryMapping& mapping) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(mapping.raw_memory_ptr()) != usages_.end());
  usages_.erase(mapping.raw_memory_ptr());
}

#if defined(CASTANETS)
void SharedMemoryTracker::OnHandleCreated(const SharedMemoryHandle& handle) {
  CHECK(handle.IsValid());
  AutoLock lock(handles_lock_);

  const UnguessableToken& guid = handle.GetGUID();
  auto it = handles_.find(guid);
  if (it != handles_.end()) {
    it->second.insert(handle.GetHandle());

    AutoLock holders_lock(holders_lock_);
    if (holders_.find(guid) != holders_.end())
      SharedMemoryTracker::RemoveHolder(guid);
  } else {
    handles_[guid].insert(handle.GetHandle());
    VLOG(2) << "Add handle" << guid << " num: " << handles_.size();
  }
}

void SharedMemoryTracker::OnHandleClosed(const SharedMemoryHandle& handle) {
  CHECK(handle.IsValid());
  AutoLock lock(handles_lock_);
  const UnguessableToken& guid = handle.GetGUID();
  auto it = handles_.find(guid);
  if (it != handles_.end()) {
    auto& fd_set = it->second;
    auto fd = fd_set.find(handle.GetHandle());
    if (fd != fd_set.end())
      fd_set.erase(fd);

    if (fd_set.empty()) {
      handles_.erase(it);
      VLOG(2) << "Del handle" << guid << " num: " << handles_.size();
    }
  }
}

void SharedMemoryTracker::AddHolder(subtle::PlatformSharedMemoryRegion handle) {
  CHECK(handle.IsValid());
  AutoLock lock(holders_lock_);
  const UnguessableToken& guid = handle.GetGUID();
  auto it = holders_.find(guid);
  if (it == holders_.end()) {
    holders_[guid] = std::move(handle);
    VLOG(1) << "Add holder" << guid << " num: " << holders_.size();
  }
}

void SharedMemoryTracker::RemoveHolder(const UnguessableToken& guid) {
  AutoLock lock(holders_lock_);
  auto holder = holders_.find(guid);
  if (holder != holders_.end()) {
    holders_.erase(holder);
    VLOG(1) << "Del holder" << guid << " num: " << holders_.size();
  }
}

int SharedMemoryTracker::Find(const UnguessableToken& guid) {
  {
    AutoLock handles_lock(handles_lock_);
    auto it = handles_.find(guid);
    if (it != handles_.end())
      return *(it->second.begin());
  }
  AutoLock holers_lock(holders_lock_);
  auto holder = holders_.find(guid);
  if (holder != holders_.end())
    return holder->second.GetPlatformHandle().fd;
  return -1;
}

const SharedMemory* SharedMemoryTracker::FindMappedMemory(
    const UnguessableToken& id) {
  AutoLock hold(usages_lock_);
  auto mapped_memory = mappings_.find(id);
  if (mapped_memory != mappings_.end())
    return mapped_memory->second;
  return nullptr;
}
#endif // defined(CASTANETS)

SharedMemoryTracker::SharedMemoryTracker() {
  trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "SharedMemoryTracker", nullptr);
}

SharedMemoryTracker::~SharedMemoryTracker() = default;

bool SharedMemoryTracker::OnMemoryDump(const trace_event::MemoryDumpArgs& args,
                                       trace_event::ProcessMemoryDump* pmd) {
  AutoLock hold(usages_lock_);
  for (const auto& usage : usages_) {
    const trace_event::MemoryAllocatorDump* dump =
        GetOrCreateSharedMemoryDumpInternal(
            usage.first, usage.second.mapped_size, usage.second.mapped_id, pmd);
    DCHECK(dump);
  }
  return true;
}

// static
const trace_event::MemoryAllocatorDump*
SharedMemoryTracker::GetOrCreateSharedMemoryDumpInternal(
    void* mapped_memory,
    size_t mapped_size,
    const UnguessableToken& mapped_id,
    trace_event::ProcessMemoryDump* pmd) {
  const std::string dump_name = GetDumpNameForTracing(mapped_id);
  trace_event::MemoryAllocatorDump* local_dump =
      pmd->GetAllocatorDump(dump_name);
  if (local_dump)
    return local_dump;

  size_t virtual_size = mapped_size;
  // If resident size is not available, a virtual size is used as fallback.
  size_t size = virtual_size;
#if defined(COUNT_RESIDENT_BYTES_SUPPORTED)
  base::Optional<size_t> resident_size =
      trace_event::ProcessMemoryDump::CountResidentBytesInSharedMemory(
          mapped_memory, mapped_size);
  if (resident_size.has_value())
    size = resident_size.value();
#endif

  local_dump = pmd->CreateAllocatorDump(dump_name);
  local_dump->AddScalar(trace_event::MemoryAllocatorDump::kNameSize,
                        trace_event::MemoryAllocatorDump::kUnitsBytes, size);
  local_dump->AddScalar("virtual_size",
                        trace_event::MemoryAllocatorDump::kUnitsBytes,
                        virtual_size);
  auto global_dump_guid = GetGlobalDumpIdForTracing(mapped_id);
  trace_event::MemoryAllocatorDump* global_dump =
      pmd->CreateSharedGlobalAllocatorDump(global_dump_guid);
  global_dump->AddScalar(trace_event::MemoryAllocatorDump::kNameSize,
                         trace_event::MemoryAllocatorDump::kUnitsBytes, size);

  // The edges will be overriden by the clients with correct importance.
  pmd->AddOverridableOwnershipEdge(local_dump->guid(), global_dump->guid(),
                                   0 /* importance */);
  return local_dump;
}

}  // namespace
