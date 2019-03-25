// Copyright 2017 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/tcp_platform_handle_utils.h"


#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

static int init = 0;
namespace mojo {
namespace edk {
namespace {

ScopedPlatformHandle CreateTCPSocket(bool needs_connection, int protocol) {
  // Create the inet socket.
  if (init == 0) {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
      printf("WSAStartup failed with error: %d\n", iResult);
      return ScopedPlatformHandle();
    }
    init = 1;
  }

  PlatformHandle socket_handle((HANDLE)socket(AF_INET, SOCK_STREAM, protocol));
  socket_handle.needs_connection = needs_connection;
  ScopedPlatformHandle handle(socket_handle);
  if (!handle.is_valid()) {
    PLOG(ERROR) << "Failed to create AF_INET socket.";
    return ScopedPlatformHandle();
  }

  // Now set it as non-blocking.
  if (false && !base::SetNonBlocking((int)handle.get().handle)) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.get().handle;
    return ScopedPlatformHandle();
  }
  return handle;
}

}  // namespace

ScopedPlatformHandle CreateTCPClientHandle(size_t port) {
  char temp_string[20];
  sprintf(temp_string, "%zu", port);
  std::string server_address = "127.0.0.1";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kServerAddress))
    server_address =
        command_line->GetSwitchValueASCII(switches::kServerAddress);
  struct addrinfo *result = NULL, hints;
  int iResult;
  // IPv4
  SOCKADDR_IN ServerAddr;
  ServerAddr.sin_family = AF_INET;

  // Port no.
  ServerAddr.sin_port = htons(port);
  // The IP address
  ServerAddr.sin_addr.s_addr = inet_addr(server_address.c_str());
  ScopedPlatformHandle handle = CreateTCPSocket(false, IPPROTO_TCP);
  if (!handle.is_valid())
    return ScopedPlatformHandle();
  if (HANDLE_EINTR(connect((SOCKET)handle.get().handle, (SOCKADDR*)&ServerAddr,
                           sizeof(ServerAddr))) < 0) {
    PLOG(ERROR) << "connect " << handle.get().handle;
    return ScopedPlatformHandle();
  }

  if (0 && !base::SetNonBlocking((int)handle.get().handle)) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.get().handle;
    // It's safe to keep listening on |server_handle| even if the attempt to set
    // O_NONBLOCK failed on the client fd.
    return ScopedPlatformHandle();
  }

  return handle;
}

ScopedPlatformHandle CreateTCPServerHandle(size_t port) {
  char temp_string[20];
  sprintf(temp_string, "%zu", port);
  struct addrinfo* result = NULL;
  struct addrinfo hints;
  int iResult;
  SOCKADDR_IN local;
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  /* Port MUST be in Network Byte Order */
  local.sin_port = htons(port);

  // TCP socket

  ScopedPlatformHandle handle = CreateTCPSocket(true, IPPROTO_TCP);
  if (!handle.is_valid())
    return ScopedPlatformHandle();

  static const int kOn = 1;
#ifdef OS_ANDROID
  setsockopt(handle.get().handle, SOL_SOCKET, 15, &kOn, sizeof(kOn));
#else
  setsockopt((SOCKET)handle.get().handle, SOL_SOCKET, SO_REUSEADDR, (char*)&kOn,
             sizeof(kOn));
#endif
  // Bind the socket.
  if (bind((SOCKET)handle.get().handle, (SOCKADDR*)&local, sizeof(local)) < 0) {
    PLOG(ERROR) << "bind " << handle.get().handle;
    return ScopedPlatformHandle();
  }

  // Start listening on the socket.
  if (listen((SOCKET)handle.get().handle, SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen" << handle.get().handle;
    return ScopedPlatformHandle();
  }
  return handle;
}

ScopedPlatformHandle CreateTCPDummyHandle() {
  PlatformHandle handle(kCastanetsHandle);
  // handle.type = PlatformHandle::Type::POSIX_CASTANETS;
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

  ScopedPlatformHandle accept_handle(PlatformHandle(
      HANDLE_EINTR((HANDLE)accept((SOCKET)server_handle.handle, NULL, 0))));
  if (!accept_handle.is_valid()) {
    PLOG(ERROR) << "accept" << server_handle.handle;
    return false;
  }

  if (0 && !base::SetNonBlocking((int)accept_handle.get().handle)) {
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