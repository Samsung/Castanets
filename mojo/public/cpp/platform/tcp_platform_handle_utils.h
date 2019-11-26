// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_
#define MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_

#include "base/component_export.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "mojo/public/cpp/platform/platform_handle.h"

#if defined(OS_WIN)
#include "base/strings/string16.h"
#endif

namespace mojo {

const size_t kCastanetsRendererPort = 8008;
const size_t kCastanetsUtilityPort = 7007;

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
PlatformHandle CreateTCPSocketHandle();

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
PlatformHandle CreateTCPClientHandle(const uint16_t port,
                                     std::string server_address = "");

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
PlatformHandle CreateTCPServerHandle(uint16_t port,
                                     uint16_t* out_port = nullptr);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool TCPServerAcceptConnection(const base::PlatformFile server_socket,
#if defined(OS_WIN)
                               base::win::ScopedHandle* handle);
#else
                               base::ScopedFD* accept_socket);
#endif
#if defined(OS_WIN)
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool TCPClientConnect(const base::win::ScopedHandle& fd,
                      std::string server_address,
                      const uint16_t port);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool IsNetworkSocket(const base::win::ScopedHandle& fd);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
std::string GetPeerAddress(const base::win::ScopedHandle& fd);
#else
COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool TCPClientConnect(const base::ScopedFD& fd,
                      std::string server_address,
                      const uint16_t port);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool IsNetworkSocket(const base::ScopedFD& fd);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
std::string GetPeerAddress(const base::ScopedFD& fd);
#endif
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_TCP_PLATFORM_HANDLE_UTILS_H_
