// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tcp_platform_handle_utils.h"

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"

#if !defined(SO_REUSEPORT)
#define SO_REUSEPORT 15
#endif

namespace mojo {
namespace {

bool ConnectRetry(int sockfd, const struct sockaddr* addr, socklen_t alen) {
  int nsec;
  for (nsec = 1; nsec <= 128; nsec <<= 1) {
    if (connect(sockfd, addr, alen) == 0) {
      return true;
    }
    if (nsec <= 128 / 2)
      sleep(nsec);
  }
  return false;
}

}  // namespace

mojo::PlatformHandle CreateTCPSocketHandle() {
  // Create the inet socket.
  PlatformHandle handle(base::ScopedFD(socket(AF_INET, SOCK_STREAM, 0)));
  if (!handle.is_valid()) {
    PLOG(ERROR) << "Failed to create AF_INET socket.";
    return PlatformHandle();
  }

  // Now set it as non-blocking.
  if (false && !base::SetNonBlocking(handle.GetFD().get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.GetFD().get();
    return PlatformHandle();
  }

  return handle;
}

PlatformHandle CreateTCPClientHandle(const uint16_t port,
                                     std::string server_address) {
  PlatformHandle handle = CreateTCPSocketHandle();
  if (!handle.is_valid())
    return PlatformHandle();

  LOG(INFO) << "Connecting TCP Socket to " << server_address << ":" << port;
  if (!TCPClientConnect(handle.GetFD(), server_address, port))
    return PlatformHandle();

  return handle;
}

bool TCPClientConnect(const base::ScopedFD& fd,
                      std::string server_address,
                      const uint16_t port) {
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
  unix_addr.sin_addr.s_addr = inet_addr(server_address.c_str());
  unix_addr_len = sizeof(struct sockaddr_in);

  if (!ConnectRetry(fd.get(), reinterpret_cast<sockaddr*>(&unix_addr),
                    unix_addr_len)) {
    PLOG(ERROR) << "Failed connect. " << fd.get();
    return false;
  }

  static const int kOn = 1;
  setsockopt(fd.get(), IPPROTO_TCP, TCP_NODELAY, &kOn, sizeof(kOn));

  LOG(INFO) << "TCP Client connected to " << server_address << ":" << port
            << ", fd:" << fd.get();
  return true;
}

PlatformHandle CreateTCPServerHandle(uint16_t port, uint16_t* out_port) {
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
  unix_addr.sin_addr.s_addr = INADDR_ANY;
  unix_addr_len = sizeof(struct sockaddr_in);

  PlatformHandle handle = CreateTCPSocketHandle();
  if (!handle.is_valid())
    return PlatformHandle();

  static const int kOn = 1;
  setsockopt(handle.GetFD().get(), SOL_SOCKET, SO_REUSEPORT, &kOn, sizeof(kOn));

  // Bind the socket.
  if (bind(handle.GetFD().get(), reinterpret_cast<const sockaddr*>(&unix_addr),
           unix_addr_len) < 0) {
    PLOG(ERROR) << "bind " << handle.GetFD().get();
    return PlatformHandle();
  }

  // Start listening on the socket.
  if (listen(handle.GetFD().get(), SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen " << handle.GetFD().get();
    return PlatformHandle();
  }

  // Get port number
  if (port == 0) {
    CHECK(out_port);
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(handle.GetFD().get(), (struct sockaddr*)&sin, &len) < 0) {
      PLOG(ERROR) << "getsockname() " << handle.GetFD().get();
      return PlatformHandle();
    }
    port = *out_port = ntohs(sin.sin_port);
  }

  LOG(INFO) << "Listen TCP Server Socket on " << port
            << " port, fd:" << handle.GetFD().get();
  return handle;
}

bool TCPServerAcceptConnection(const base::PlatformFile server_socket,
                               base::ScopedFD* accept_socket) {
  DCHECK(server_socket);
  accept_socket->reset();
#if defined(OS_NACL)
  NOTREACHED();
  return false;
#else
  int accept_fd = accept(server_socket, NULL, 0);
  base::ScopedFD accept_handle(
      base::PlatformFile(HANDLE_EINTR(accept_fd)));
  if (!accept_handle.is_valid()) {
    PLOG(ERROR) << "accept " << server_socket;
    return false;
  }

  if (false && !base::SetNonBlocking(accept_handle.get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed "
                << accept_handle.get();
    // It's safe to keep listening on |server_handle| even if the attempt to set
    // O_NONBLOCK failed on the client fd.
    return true;
  }

  static const int kOn = 1;
  setsockopt(accept_handle.get(), IPPROTO_TCP, TCP_NODELAY, &kOn, sizeof(kOn));

  *accept_socket = std::move(accept_handle);
  return true;
#endif  // defined(OS_NACL)
}

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool IsNetworkSocket(const base::ScopedFD& fd) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (!getsockname(fd.get(), (struct sockaddr*)&addr, &len)) {
    return (addr.ss_family == AF_INET);
  }
  return false;
}

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
std::string GetPeerAddress(const base::ScopedFD& fd) {
  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(struct sockaddr_in);
  if (!getpeername(fd.get(), (struct sockaddr*)&addr, &addr_size))
    return inet_ntoa(addr.sin_addr);
  return std::string();
}

}  // namespace mojo
