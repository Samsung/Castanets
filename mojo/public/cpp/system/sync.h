// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_SYNC_H_
#define MOJO_PUBLIC_CPP_SYSTEM_SYNC_H_

#include "base/unguessable_token.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/system_export.h"
#include "mojo/public/c/system/sync.h"

namespace mojo {

MOJO_CPP_SYSTEM_EXPORT MojoResult
SyncSharedMemory(const base::UnguessableToken& guid,
                 size_t offset,
                 size_t sync_size,
                 BrokerCompressionMode compression_mode = BrokerCompressionMode::ZLIB);

MOJO_CPP_SYSTEM_EXPORT MojoResult
SyncSharedMemory2d(const base::UnguessableToken& guid,
                   size_t width,
                   size_t height,
                   size_t bytes_per_pixel,
                   size_t offset = 0,
                   size_t stride = 0,
                   BrokerCompressionMode compression_mode = BrokerCompressionMode::WEBP);

MOJO_CPP_SYSTEM_EXPORT MojoResult
WaitSyncSharedMemory(const base::UnguessableToken& guid);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_SYNC_H_
