// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_
#define MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_

#include "build/build_config.h"
#include "mojo/core/system_impl_export.h"
#include "base/component_export.h"

#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"

#if defined(OS_WIN)
#include "base/strings/string16.h"
#endif

namespace mojo {

const size_t kCastanetsSyncPort = 8008;
const size_t kCastanetsBrokerPort = 9009;
const size_t kCastanetsUtilitySyncPort = 7007;
const size_t kCastanetsUtilityBrokerPort = 6006;
const size_t kCastanetsNonBrokerPort = 5005;

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
base::ScopedFD
CreateTCPClientHandle(size_t port);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
base::ScopedFD
CreateTCPServerHandle(size_t port);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
base::ScopedFD
CreateTCPDummyHandle();

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool TCPServerAcceptConnection(
        base::PlatformFile server_fd,
        base::ScopedFD* connection_fd);

}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_
