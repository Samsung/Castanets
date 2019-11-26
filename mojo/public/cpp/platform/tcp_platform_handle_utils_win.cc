// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tcp_platform_handle_utils.h"

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#include <errno.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"

static int init = 0;
namespace mojo {

PlatformHandle CreateTCPSocketHandle() {
  // Create the inet socket.
  if (init == 0) {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
      LOG(ERROR) << "WSAStartup failed with error: "<< iResult;
      return PlatformHandle();
    }
    init = 1;
  }
  PlatformHandle handle(base::win::ScopedHandle((HANDLE)socket(AF_INET, SOCK_STREAM, 0)));
  //socket_handle.needs_connection = needs_connection;
  if (!handle.is_valid()) {
    PLOG(ERROR) << "Failed to create AF_INET socket.";
    return PlatformHandle();
  }

  // Now set it as non-blocking.
  if (0 && !base::SetNonBlocking((int)handle.GetHandle().Get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << handle.GetHandle().Get();
    return PlatformHandle();
  }
  return handle;
}

PlatformHandle CreateTCPClientHandle(const uint16_t port,
                                     std::string server_address) {
  char temp_string[20];
  sprintf(temp_string, "%zu", port);
  if (server_address.empty()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kServerAddress))
      server_address =
          command_line->GetSwitchValueASCII(switches::kServerAddress);
    else
      server_address = "127.0.0.1";
  }
  PlatformHandle handle = CreateTCPSocketHandle();
  if (!handle.is_valid())
    return PlatformHandle();

  LOG(INFO) << "Connecting TCP Socket to " << server_address << ":" << port;
  if (!TCPClientConnect(handle.GetHandle(), server_address, port))
    return PlatformHandle();
  return handle;
}
bool TCPClientConnect(const base::win::ScopedHandle& fd,
                      std::string server_address,
                      const uint16_t port) {
  struct addrinfo *result = NULL, hints;
  int iResult;
  // IPv4
  SOCKADDR_IN ServerAddr;
  ServerAddr.sin_family = AF_INET;
  // Port no.
  ServerAddr.sin_port = htons(port);
  // The IP address
  ServerAddr.sin_addr.s_addr = inet_addr(server_address.c_str());

  if (HANDLE_EINTR(connect((SOCKET)fd.Get(), (SOCKADDR*)&ServerAddr,
                       sizeof(ServerAddr))) < 0) {
    PLOG(ERROR) << "connect " << fd.Get();
    return false;
  }

  //static const int kOn = 1;
  //setsockopt(fd.Get(), IPPROTO_TCP, TCP_NODELAY, &kOn, sizeof(kOn));

  LOG(INFO) << "TCP Client connected to " << server_address << ":" << port
            << ", fd:" << fd.Get();
  return true;
}

PlatformHandle CreateTCPServerHandle(uint16_t port, uint16_t* out_port) {
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
  PlatformHandle handle = CreateTCPSocketHandle();
  if (!handle.is_valid())
    return PlatformHandle();

  static const int kOn = 1;
#ifdef OS_ANDROID
  setsockopt(handle.get().handle, SOL_SOCKET, 15, &kOn, sizeof(kOn));
#else
  setsockopt((SOCKET)handle.GetHandle().Get(), SOL_SOCKET, SO_REUSEADDR, (char*)&kOn,
             sizeof(kOn));
#endif
  // Bind the socket.
  if (bind((SOCKET)handle.GetHandle().Get(), (SOCKADDR*)&local, sizeof(local)) < 0) {
    PLOG(ERROR) << "bind " << handle.GetHandle().Get();
    return PlatformHandle();
  }

  // Start listening on the socket.
  if (listen((SOCKET)handle.GetHandle().Get(), SOMAXCONN) < 0) {
    PLOG(ERROR) << "listen" << handle.GetHandle().Get();
    return PlatformHandle();
  }

  // Get port number
  if (port == 0) {
    CHECK(out_port);
    SOCKADDR_IN sin;
    socklen_t len = sizeof(sin);
    if (getsockname((SOCKET)handle.GetHandle().Get(), (struct sockaddr*)&sin, &len) < 0) {
      PLOG(ERROR) << "getsockname() " << handle.GetHandle().Get();
	  return PlatformHandle();
    }
    port = *out_port = ntohs(sin.sin_port);
  }
  return handle;
}

bool TCPServerAcceptConnection(const base::PlatformFile server_socket,
#if defined(OS_WIN)
                               base::win::ScopedHandle* accept_socket) {
#else
                               base::ScopedFD* accept_socket) {
#endif
  DCHECK(server_socket);
  accept_socket->Close();

#if defined(OS_NACL)
  NOTREACHED();
  return false;
#else
  HANDLE handle = (HANDLE)accept((SOCKET)server_socket, NULL, 0);
  base::win::ScopedHandle accept_handle(handle);
  if (!accept_handle.IsValid()) {
    PLOG(ERROR) << "accept " << server_socket;
    return false;
  }

  if (0 && !base::SetNonBlocking((int)accept_handle.Get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed "
                << accept_handle.Get();
    // It's safe to keep listening on |server_handle| even if the attempt to set
    // O_NONBLOCK failed on the client fd.
    return true;
  }

  *accept_socket = std::move(accept_handle);

  return true;
#endif  // defined(OS_NACL)
}

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
bool IsNetworkSocket(const base::win::ScopedHandle& fd) {
  // TODO (suyambu.rm) check why this method returns false
  return true;
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (!getsockname((int)fd.Get(), (struct sockaddr*)&addr, &len)) {
    return (addr.ss_family == AF_INET);
  }
  return false;
}

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
std::string GetPeerAddress(const base::win::ScopedHandle& fd) {
  SOCKADDR_IN addr;
  socklen_t addr_size = sizeof(addr);
  if (!getpeername((int)fd.Get(), (struct sockaddr*)&addr, &addr_size))
    return inet_ntoa(addr.sin_addr);
  return std::string();
}

}  // namespace mojo

