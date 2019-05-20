// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tcp_platform_handle_utils.h"

#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "mojo/public/c/system/platform_handle.h"

#include "base/debug/stack_trace.h"
#include <unistd.h>
#include <fcntl.h>
 
namespace mojo {
namespace {


base::ScopedFD CreateTCPSocket(bool needs_connection, int protocol) {
  // Create the inet socket.
  base::PlatformFile socket_handle(socket(AF_INET, SOCK_STREAM, protocol));
  base::ScopedFD handle(socket_handle);
  if (handle.get() < 0) {
    PLOG(ERROR) << "Failed to create AF_INET socket.";
    return base::ScopedFD();
  }

  // Now set it as non-blocking.
  if (false && !base::SetNonBlocking(handle.get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.get();
    return base::ScopedFD();
  }
  return handle;
}

}  // namespace

int
connect_retry(int sockfd, const struct sockaddr *addr, socklen_t alen)
{
    int nsec;
    for (nsec = 1; nsec <= 128; nsec <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            return(0);
        }
        if (nsec <= 128/2)
            sleep(nsec);
    }
    return(-1);
}

base::ScopedFD CreateTCPClientHandle(size_t port) {
  std::string server_address = "127.0.0.1";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kServerAddress))
    server_address = command_line->GetSwitchValueASCII(switches::kServerAddress);
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

  base::ScopedFD handle = CreateTCPSocket(false, IPPROTO_TCP);
  LOG(INFO) << "Client Sock fd for port " << port <<":"<<handle.get();
  if (handle.get() < 0)
    return base::ScopedFD();
  if (HANDLE_EINTR(connect_retry(handle.get(),
      reinterpret_cast<sockaddr*>(&unix_addr), unix_addr_len)) < 0) {
    PLOG(ERROR) << "connect " << handle.get();
    return base::ScopedFD();
  }
  return handle;
}

base::ScopedFD CreateTCPServerHandle(size_t port) {
  struct sockaddr_in unix_addr;
  size_t unix_addr_len;
  memset(&unix_addr, 0, sizeof(struct sockaddr_in));
  unix_addr.sin_family = AF_INET;
  unix_addr.sin_port = htons(port);
  unix_addr.sin_addr.s_addr = INADDR_ANY;
  unix_addr_len = sizeof(struct sockaddr_in);

  base::ScopedFD handle = CreateTCPSocket(true, 0);
  if (!handle.get())
    return base::ScopedFD();

  static const int kOn = 1;
#ifdef OS_ANDROID
  setsockopt(handle.get(), SOL_SOCKET, 15, &kOn, sizeof(kOn));
#else
  setsockopt(handle.get(), SOL_SOCKET, SO_REUSEPORT, &kOn, sizeof(kOn));
#endif

  // Bind the socket.
  if (bind(handle.get(), reinterpret_cast<const sockaddr*>(&unix_addr),
           unix_addr_len) < 0) {
    PLOG(ERROR) << "bind " << handle.get();
    return base::ScopedFD();
  }

  // Start listening on the socket.
  if (listen(handle.get(), SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen" << handle.get();
    return base::ScopedFD();
  }
  LOG(INFO) << "Server Sock fd for port " << port <<":"<<handle.get();
  return handle;
}

base::ScopedFD CreateTCPDummyHandle() {
  base::PlatformFile handle(std::move(kCastanetsHandle));
  //handle.type = PlatformHandle::Type::POSIX_CHROMIE;
  return base::ScopedFD(handle);
}

bool TCPServerAcceptConnection(base::PlatformFile server_handle,
                               base::ScopedFD* connection_handle) {
  DCHECK(server_handle);
  connection_handle->reset();
#if defined(OS_NACL)
  NOTREACHED();
  return false;
#else
  int accept_fd = accept(server_handle, NULL, 0);
  base::ScopedFD accept_handle(
      base::PlatformFile(HANDLE_EINTR(accept_fd)));
  if (!accept_handle.is_valid()) {
    PLOG(ERROR) << "accept" << server_handle;
    return false;
  }

  if (false && !base::SetNonBlocking(accept_handle.get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed "
                << accept_handle.get();
    // It's safe to keep listening on |server_handle| even if the attempt to set
    // O_NONBLOCK failed on the client fd.
    return true;
  }

  *connection_handle = std::move(accept_handle);
  return true;
#endif  // defined(OS_NACL)
}

}  // namespace mojo
