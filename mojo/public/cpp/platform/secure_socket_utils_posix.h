// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_PLATFORM_SECURE_SOCKET_UTILS_POSIX_H_
#define MOJO_PUBLIC_CPP_PLATFORM_SECURE_SOCKET_UTILS_POSIX_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"

#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace mojo {

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
SSL* AcceptSSLConnection(int socket);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
SSL* ConnectSSLConnection(int socket);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
ssize_t SecureSocketWrite(SSL* ssl, const void* bytes, size_t num_bytes);

COMPONENT_EXPORT(MOJO_CPP_PLATFORM)
ssize_t SecureSocketRecvmsg(SSL* socket, void* buf, size_t num_bytes);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_PLATFORM_SECURE_SOCKET_UTILS_POSIX_H_
