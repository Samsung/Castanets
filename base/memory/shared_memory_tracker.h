// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_TRACKER_H_
#define BASE_MEMORY_SHARED_MEMORY_TRACKER_H_

#include <map>
#include <string>

#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"

namespace base {

namespace trace_event {
class MemoryAllocatorDump;
class MemoryAllocatorDumpGuid;
class ProcessMemoryDump;
}

#if defined(CASTANETS)
class CastanetsMemoryHolder;
class CastanetsMemoryMapping;
class CastanetsMemorySyncer;
class SyncDelegate;
class UnknownMemorySyncer;
#endif

// SharedMemoryTracker tracks shared memory usage.
class BASE_EXPORT SharedMemoryTracker : public trace_event::MemoryDumpProvider {
 public:
  // Returns a singleton instance.
  static SharedMemoryTracker* GetInstance();

  static std::string GetDumpNameForTracing(const UnguessableToken& id);

  static trace_event::MemoryAllocatorDumpGuid GetGlobalDumpIdForTracing(
      const UnguessableToken& id);

  // Gets or creates if non-existant, a memory dump for the |shared_memory|
  // inside the given |pmd|. Also adds the necessary edges for the dump when
  // creating the dump.
  static const trace_event::MemoryAllocatorDump* GetOrCreateSharedMemoryDump(
      const SharedMemory* shared_memory,
      trace_event::ProcessMemoryDump* pmd);
  // We're in the middle of a refactor https://crbug.com/795291. Eventually, the
  // first call will go away.
  static const trace_event::MemoryAllocatorDump* GetOrCreateSharedMemoryDump(
      const SharedMemoryMapping& shared_memory,
      trace_event::ProcessMemoryDump* pmd);

  // Records shared memory usage on valid mapping.
  void IncrementMemoryUsage(const SharedMemory& shared_memory);
  void IncrementMemoryUsage(const SharedMemoryMapping& mapping);

  // Records shared memory usage on unmapping.
  void DecrementMemoryUsage(const SharedMemory& shared_memory);
  void DecrementMemoryUsage(const SharedMemoryMapping& mapping);

#if defined(CASTANETS)
  void AddHolder(subtle::PlatformSharedMemoryRegion handle);
  void RemoveHolder(const UnguessableToken& guid);

  subtle::PlatformSharedMemoryRegion FindMemoryHolder(
      const UnguessableToken& id);

  scoped_refptr<CastanetsMemoryMapping> FindMappedMemory(
      const UnguessableToken& id);

  SyncDelegate* FindCreatedBuffer(const UnguessableToken& id);

  void AddFDInTransit(const UnguessableToken& guid, int fd);

  void MapExternalMemory(int fd, SyncDelegate* delegate);
  void MapInternalMemory(int fd);
  CastanetsMemorySyncer* MapExternalMemory(const UnguessableToken& guid,
                                           SyncDelegate* delegate);

  void OnBufferCreated(const UnguessableToken& guid, SyncDelegate* syncer);

  CastanetsMemorySyncer* GetSyncer(const UnguessableToken& guid);
#endif

  // Root dump name for all shared memory dumps.
  static const char kDumpRootName[];

 private:
  SharedMemoryTracker();
  ~SharedMemoryTracker() override;

  // trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const trace_event::MemoryDumpArgs& args,
                    trace_event::ProcessMemoryDump* pmd) override;

  static const trace_event::MemoryAllocatorDump*
  GetOrCreateSharedMemoryDumpInternal(void* mapped_memory,
                                      size_t mapped_size,
                                      const UnguessableToken& mapped_id,
                                      trace_event::ProcessMemoryDump* pmd);

  // Information associated with each mapped address.
  struct UsageInfo {
    UsageInfo(size_t size, const UnguessableToken& id)
        : mapped_size(size), mapped_id(id) {}

    size_t mapped_size;
    UnguessableToken mapped_id;
  };

  // Used to lock when |usages_| is modified or read.
  Lock usages_lock_;
  std::map<void*, UsageInfo> usages_;

#if defined(CASTANETS)
  friend class CastanetsMemoryHolder;

  void AddMapping(const UnguessableToken& guid, size_t size, void* ptr);
  void RemoveMapping(const UnguessableToken& guid, void* ptr);

  std::unique_ptr<UnknownMemorySyncer> TakeUnknownMemory(int fd);

  Lock mapping_lock_;
  std::map<UnguessableToken, scoped_refptr<CastanetsMemoryMapping>> mappings_;

  Lock unknown_lock_;
  std::map<UnguessableToken, std::unique_ptr<UnknownMemorySyncer>>
      unknown_memories_;

  Lock syncer_lock_;
  std::map<UnguessableToken, std::unique_ptr<CastanetsMemorySyncer>>
      memory_syncers_;

  Lock holders_lock_;
  std::map<UnguessableToken, CastanetsMemoryHolder> holders_;

  Lock created_buffer_lock_;
  std::map<UnguessableToken, SyncDelegate*> created_buffers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryTracker);
};

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_TRACKER_H_
