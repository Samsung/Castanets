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


mojo::PlatformHandle CreateTCPSocket(bool needs_connection, int protocol) {
  // Create the inet socket.
  PlatformHandle handle(base::ScopedFD(socket(AF_INET, SOCK_STREAM, protocol)));
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

}  // namespace

int connect_retry(int sockfd, const struct sockaddr* addr, socklen_t alen) {
  int nsec;
  for (nsec = 1; nsec <= 128; nsec <<= 1) {
    if (connect(sockfd, addr, alen) == 0) {
      return (0);
    }
    if (nsec <= 128 / 2)
      sleep(nsec);
  }
  return (-1);
}

PlatformHandle CreateTCPClientHandle(const uint16_t port,
                                     std::string server_address) {
  if (server_address.empty()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kServerAddress))
      server_address =
          command_line->GetSwitchValueASCII(switches::kServerAddress);
    else
      server_address = "127.0.0.1";
  }
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
#ifdef OS_ANDROID
  unix_addr.sin_addr.s_addr = inet_addr("192.168.0.118");
#else
  unix_addr.sin_addr.s_addr = inet_addr(server_address.c_str());
#endif
  if (port == 5005)
    unix_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  unix_addr_len = sizeof(struct sockaddr_in);

  PlatformHandle handle = CreateTCPSocket(false, IPPROTO_TCP);
  if (!handle.is_valid())
    return PlatformHandle();

  static const int kOn = 1;
  setsockopt(handle.GetFD().get(), IPPROTO_TCP, TCP_NODELAY, &kOn, sizeof(kOn));


  if (HANDLE_EINTR(connect_retry(handle.GetFD().get(),
                                 reinterpret_cast<sockaddr*>(&unix_addr),
                                 unix_addr_len)) < 0) {
    PLOG(ERROR) << "Failed connect. " << handle.GetFD().get();
    return PlatformHandle();
  }

  LOG(INFO) << "Client Sock fd for port " << port << ":"
            << handle.GetFD().get();
  return handle;
}

PlatformHandle CreateTCPServerHandle(uint16_t port, uint16_t *out_port) {
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
  unix_addr.sin_addr.s_addr = INADDR_ANY;
  unix_addr_len = sizeof(struct sockaddr_in);

  PlatformHandle handle = CreateTCPSocket(true, 0);
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
    PLOG(ERROR) << "listen" << handle.GetFD().get();
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
  LOG(INFO) << "Server Sock fd for port " << port << ":"
            << handle.GetFD().get();
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
  // FIXME:Do a dummy open to avoid allocating 0 to sock fd
  int accept_fd = accept(server_socket, NULL, 0);
  base::ScopedFD accept_handle(
      base::PlatformFile(HANDLE_EINTR(accept_fd)));
  if (!accept_handle.is_valid()) {
    PLOG(ERROR) << "accept" << server_socket;
    return false;
  }

  //printf("%s %s %d serverhandle %d accept handle %d \n",__FILE__,__func__,__LINE__, server_handle, accept_handle.get());
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

}  // namespace mojo
