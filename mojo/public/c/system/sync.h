// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_C_SYSTEM_SYNC_H_
#define MOJO_PUBLIC_C_SYSTEM_SYNC_H_

#include <stdint.h>

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum BrokerCompressionMode : uint32_t { NONE, ZLIB, WEBP };

MOJO_SYSTEM_EXPORT MojoResult MojoSyncPlatformSharedMemoryRegion(
    const struct MojoSharedBufferGuid* guid,
    size_t offset,
    size_t sync_size,
    BrokerCompressionMode compression_mode = BrokerCompressionMode::ZLIB);

MOJO_SYSTEM_EXPORT MojoResult MojoSyncPlatformSharedMemoryRegion2d(
    const struct MojoSharedBufferGuid* guid,
    size_t width,
    size_t height,
    size_t bytes_per_pixel,
    size_t offset = 0,
    size_t stride = 0,
    BrokerCompressionMode compression_mode = BrokerCompressionMode::WEBP);

MOJO_SYSTEM_EXPORT MojoResult
MojoWaitSyncPlatformSharedMemoryRegion(const struct MojoSharedBufferGuid* guid);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_SYNC_H_
