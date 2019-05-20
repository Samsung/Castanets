// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/tcp_platform_handle_utils.h"

#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/distributed_chromium_util.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace mojo {
namespace edk {
namespace {


ScopedPlatformHandle CreateTCPSocket(bool needs_connection, int protocol) {
  // Create the inet socket.
  PlatformHandle socket_handle(socket(AF_INET, SOCK_STREAM, protocol));
  socket_handle.needs_connection = needs_connection;
  ScopedPlatformHandle handle(socket_handle);
  if (!handle.is_valid()) {
    PLOG(ERROR) << "Failed to create AF_INET socket.";
    return ScopedPlatformHandle();
  }

  // Now set it as non-blocking.
  if (false && !base::SetNonBlocking(handle.get().handle)) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.get().handle;
    return ScopedPlatformHandle();
  }
  return handle;
}

}  // namespace

ScopedPlatformHandle CreateTCPClientHandle(size_t port) {
  std::string server_address = base::Castanets::ServerAddress();
  struct addrinfo* result = NULL;
  int status = getaddrinfo(server_address.c_str(), NULL, NULL, &result);
  if (status == 0 && result != NULL) {
    char host[NI_MAXHOST] = "";
    status = getnameinfo(result->ai_addr, result->ai_addrlen, host,
                         NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if (status == 0 && *host) {
      server_address = std::string(host);
    }
    freeaddrinfo(result);
  }
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
  unix_addr.sin_addr.s_addr = inet_addr(server_address.c_str());
  unix_addr_len = sizeof(struct sockaddr_in);

  ScopedPlatformHandle handle = CreateTCPSocket(false, IPPROTO_TCP);
  if (!handle.is_valid())
    return ScopedPlatformHandle();

  if (HANDLE_EINTR(connect(handle.get().handle,
                           reinterpret_cast<sockaddr*>(&unix_addr),
                           unix_addr_len)) < 0) {
    PLOG(ERROR) << "connect " << handle.get().handle;
    return ScopedPlatformHandle();
  }
  return handle;
}

ScopedPlatformHandle CreateTCPServerHandle(size_t port) {
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
  unix_addr.sin_addr.s_addr = INADDR_ANY;
  unix_addr_len = sizeof(struct sockaddr_in);

  ScopedPlatformHandle handle = CreateTCPSocket(true, 0);
  if (!handle.is_valid())
    return ScopedPlatformHandle();

  static const int kOn = 1;
#ifdef OS_ANDROID
  setsockopt(handle.get().handle, SOL_SOCKET, 15, &kOn, sizeof(kOn));
#else
  setsockopt(handle.get().handle, SOL_SOCKET, SO_REUSEPORT, &kOn, sizeof(kOn));
#endif

  // Bind the socket.
  if (bind(handle.get().handle, reinterpret_cast<const sockaddr*>(&unix_addr),
           unix_addr_len) < 0) {
    PLOG(ERROR) << "bind " << handle.get().handle;
    return ScopedPlatformHandle();
  }

  // Start listening on the socket.
  if (listen(handle.get().handle, SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen" << handle.get().handle;
    return ScopedPlatformHandle();
  }

  return handle;
}

ScopedPlatformHandle CreateTCPDummyHandle() {
  PlatformHandle handle(kCastanetsHandle);
  handle.type = PlatformHandle::Type::POSIX_CASTANETS;
  return ScopedPlatformHandle(handle);
}

bool TCPServerAcceptConnection(PlatformHandle server_handle,
                               ScopedPlatformHandle* connection_handle) {
  DCHECK(server_handle.is_valid());
  connection_handle->reset();
#if defined(OS_NACL)
  NOTREACHED();
  return false;
#else
  ScopedPlatformHandle accept_handle(
      PlatformHandle(HANDLE_EINTR(accept(server_handle.handle, NULL, 0))));
  if (!accept_handle.is_valid()) {
    PLOG(ERROR) << "accept" << server_handle.handle;
    return false;
  }

  if (!base::SetNonBlocking(accept_handle.get().handle)) {
    PLOG(ERROR) << "base::SetNonBlocking() failed "
                << accept_handle.get().handle;
    // It's safe to keep listening on |server_handle| even if the attempt to set
    // O_NONBLOCK failed on the client fd.
    return true;
  }

  *connection_handle = std::move(accept_handle);
  return true;
#endif  // defined(OS_NACL)
}

}  // namespace edk
}  // namespace mojo
