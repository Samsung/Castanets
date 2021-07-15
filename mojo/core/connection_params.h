// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CONNECTION_PARAMS_H_
#define MOJO_CORE_CONNECTION_PARAMS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

namespace mojo {
namespace core {

// A set of parameters used when establishing a connection to another process.
class MOJO_SYSTEM_IMPL_EXPORT ConnectionParams {
 public:
  ConnectionParams();
  explicit ConnectionParams(PlatformChannelEndpoint endpoint);
  explicit ConnectionParams(PlatformChannelServerEndpoint server_endpoint);
  ConnectionParams(ConnectionParams&&);
  ~ConnectionParams();

  ConnectionParams& operator=(ConnectionParams&&);

  const PlatformChannelEndpoint& endpoint() const { return endpoint_; }
  const PlatformChannelServerEndpoint& server_endpoint() const {
    return server_endpoint_;
  }

  PlatformChannelEndpoint TakeEndpoint() { return std::move(endpoint_); }

  PlatformChannelServerEndpoint TakeServerEndpoint() {
    return std::move(server_endpoint_);
  }

  void set_is_async(bool is_async) { is_async_ = is_async; }
  bool is_async() const { return is_async_; }

  void set_leak_endpoint(bool leak_endpoint) { leak_endpoint_ = leak_endpoint; }
  bool leak_endpoint() const { return leak_endpoint_; }

#if defined(CASTANETS)
  bool is_secure() const { return secure_connection_; }
  void SetSecure(bool secure_connection = true) {
    secure_connection_ = secure_connection;
  }

  const std::string& tcp_address() const { return tcp_address_; }
  uint16_t tcp_port() const { return tcp_port_; }
  void SetTcpClient(std::string address, uint16_t port) {
    tcp_address_ = address;
    tcp_port_ = port;
  }

 private:
  bool secure_connection_ = false;
  std::string tcp_address_;
  uint16_t tcp_port_ = 0;
#endif

 private:
  bool is_async_ = false;
  bool leak_endpoint_ = false;
  PlatformChannelEndpoint endpoint_;
  PlatformChannelServerEndpoint server_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionParams);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CONNECTION_PARAMS_H_
