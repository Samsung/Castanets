// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_
#define MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_

#include "build/build_config.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"

#if defined(OS_WIN)
#include "base/strings/string16.h"
#endif

namespace mojo {
namespace edk {

const size_t kCastanetsAudioSyncPort = 7000;
const size_t kCastanetsSyncPort = 8880;
const size_t kCastanetsUtilitySyncPort = 6000;
const size_t kCastanetsBrokerPort = 9990;

MOJO_SYSTEM_IMPL_EXPORT ScopedPlatformHandle
CreateTCPClientHandle(size_t port);

MOJO_SYSTEM_IMPL_EXPORT ScopedPlatformHandle
CreateTCPServerHandle(size_t port);

MOJO_SYSTEM_IMPL_EXPORT ScopedPlatformHandle
CreateTCPDummyHandle();

MOJO_SYSTEM_IMPL_EXPORT bool TCPServerAcceptConnection(
    PlatformHandle server_handle,
    ScopedPlatformHandle* connection_handle);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_
