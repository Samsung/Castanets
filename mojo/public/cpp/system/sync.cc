// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/sync.h"

#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/sync.h"

namespace mojo {

MojoResult SyncSharedMemory(const base::UnguessableToken& guid,
                            size_t offset,
                            size_t sync_size) {
  MojoSharedBufferGuid mojo_guid;
  mojo_guid.high = guid.GetHighForSerialization();
  mojo_guid.low = guid.GetLowForSerialization();

  return MojoSyncPlatformSharedMemoryRegion(&mojo_guid, offset, sync_size);
}

MojoResult SyncSharedMemory2d(const base::UnguessableToken& guid,
                              size_t offset,
                              size_t sync_size,
                              size_t width,
                              size_t stride) {
  MojoSharedBufferGuid mojo_guid;
  mojo_guid.high = guid.GetHighForSerialization();
  mojo_guid.low = guid.GetLowForSerialization();

  return MojoSyncPlatformSharedMemoryRegion2d(&mojo_guid, offset, sync_size,
                                              width, stride);
}

MojoResult WaitSyncSharedMemory(const base::UnguessableToken& guid) {
  MojoSharedBufferGuid mojo_guid;
  mojo_guid.high = guid.GetHighForSerialization();
  mojo_guid.low = guid.GetLowForSerialization();

  return MojoWaitSyncPlatformSharedMemoryRegion(&mojo_guid);
}

}  // namespace mojo