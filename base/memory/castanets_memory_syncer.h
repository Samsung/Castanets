// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_CASTANETS_MEMORY_SYNCER_H_
#define BASE_MEMORY_CASTANETS_MEMORY_SYNCER_H_

#include <memory>

#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"

namespace base {

class ExternalMemorySyncer;
class CastanetsMemoryMapping;

class BASE_EXPORT SyncDelegate {
 public:
  virtual ~SyncDelegate() {}

  virtual void SendSyncEvent(
      scoped_refptr<base::CastanetsMemoryMapping> mapping_info,
      size_t offset,
      size_t sync_size) = 0;
};

class BASE_EXPORT CastanetsMemorySyncer {
 public:
  virtual void SyncMemory(size_t offset, size_t sync_size) = 0;
  virtual ~CastanetsMemorySyncer() = default;
};

class UnknownMemorySyncer : public CastanetsMemorySyncer {
 public:
  UnknownMemorySyncer(scoped_refptr<CastanetsMemoryMapping> mapping_info);
  UnknownMemorySyncer(int fd);
  UnknownMemorySyncer(scoped_refptr<CastanetsMemoryMapping> mapping_info,
                      int fd);
  ~UnknownMemorySyncer() override;

  int GetFD() const { return fd_in_transit_; }
  UnguessableToken GetGUID() const;
  bool HasPendingSyncs() const { return !pending_syncs_.empty(); }

  void SetMappingInfo(scoped_refptr<CastanetsMemoryMapping> mapping_info);
  void SetFdInTransit(int fd);

  std::unique_ptr<ExternalMemorySyncer> ConvertToExternal(
      SyncDelegate* delegate);

  scoped_refptr<CastanetsMemoryMapping> GetMappingInfo() {
    return mapping_info_;
  }

  // base::CastanetsMemorySyncer:
  void SyncMemory(size_t offset, size_t sync_size) override;

 private:
  scoped_refptr<CastanetsMemoryMapping> mapping_info_;

  int fd_in_transit_;

  struct SyncInfo {
    size_t offset;
    size_t size;
  };
  std::vector<SyncInfo> pending_syncs_;

  DISALLOW_COPY_AND_ASSIGN(UnknownMemorySyncer);
};

class ExternalMemorySyncer : public CastanetsMemorySyncer {
 public:
  ExternalMemorySyncer(SyncDelegate* delegate,
                       scoped_refptr<CastanetsMemoryMapping> mapping);

  ~ExternalMemorySyncer() override;

  void SyncMemory(size_t offset, size_t sync_size) override;

 private:
  SyncDelegate* const delegate_;
  scoped_refptr<CastanetsMemoryMapping> mapping_info_;

  DISALLOW_COPY_AND_ASSIGN(ExternalMemorySyncer);
};

}  // namespace base

#endif  // BASE_MEMORY_CASTANETS_MEMORY_SYNCER_H_
