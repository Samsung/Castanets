// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/secure_socket_utils_posix.h"

#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "crypto/openssl_util.h"

namespace mojo {

namespace {

constexpr char kTestCertFileName[] = "/localhost_cert.pem";

base::FilePath GetTestCertsDirectory() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("net"));
  path = path.Append(FILE_PATH_LITERAL("data"));
  path = path.Append(FILE_PATH_LITERAL("ssl"));
  path = path.Append(FILE_PATH_LITERAL("certificates"));
  return path;
}

class SSLServerContext {
 public:
  static SSLServerContext* GetInstance() {
    return base::Singleton<SSLServerContext,
                           base::LeakySingletonTraits<SSLServerContext>>::get();
  }
  SSL_CTX* ssl_ctx() { return ssl_ctx_.get(); }

 private:
  friend struct base::DefaultSingletonTraits<SSLServerContext>;

  SSLServerContext() {
    crypto::EnsureOpenSSLInit();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx_.reset(SSL_CTX_new(TLS_server_method()));
    CHECK(ssl_ctx_);

    std::string cert_file(GetTestCertsDirectory().value() + kTestCertFileName);
    LOG(INFO) << "SSL certificate file: " << cert_file;

    CHECK_GT(SSL_CTX_use_certificate_file(ssl_ctx_.get(),
                                          cert_file.c_str(),
                                          SSL_FILETYPE_PEM), 0);

    CHECK_GT(SSL_CTX_use_PrivateKey_file(ssl_ctx_.get(),
                                         cert_file.c_str(),
                                         SSL_FILETYPE_PEM), 0);

    CHECK(SSL_CTX_check_private_key(ssl_ctx_.get()));
  }

  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
};

class SSLClientContext {
 public:
  static SSLClientContext* GetInstance() {
    return base::Singleton<SSLClientContext,
                           base::LeakySingletonTraits<SSLClientContext>>::get();
  }
  SSL_CTX* ssl_ctx() { return ssl_ctx_.get(); }

 private:
  friend struct base::DefaultSingletonTraits<SSLClientContext>;

  SSLClientContext() {
    crypto::EnsureOpenSSLInit();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx_.reset(SSL_CTX_new(TLS_client_method()));
    CHECK(ssl_ctx_);
  }

  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
};

} // namespace

SSL* AcceptSSLConnection(int socket) {
  CHECK_GE(socket, 0);
  SSL* ssl = SSL_new(SSLServerContext::GetInstance()->ssl_ctx());
  CHECK(ssl);
  SSL_set_fd(ssl, socket);
  SSL_accept(ssl);
  return ssl;
}

SSL* ConnectSSLConnection(int socket) {
  CHECK_GE(socket, 0);
  SSL* ssl = SSL_new(SSLClientContext::GetInstance()->ssl_ctx());
  CHECK(ssl);
  SSL_set_fd(ssl, socket);
  SSL_connect(ssl);
  return ssl;
}

ssize_t SecureSocketWrite(SSL* ssl, const void* bytes, size_t num_bytes) {
  return SSL_write(ssl, bytes, num_bytes);
}

ssize_t SecureSocketRecvmsg(SSL* ssl, void* buf, size_t num_bytes) {
  return SSL_read(ssl, buf, num_bytes);
}

}  // namespace mojo
