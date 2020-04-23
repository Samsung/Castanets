// Copyright 2019 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/secure_socket_utils_posix.h"

#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

#if defined(OS_TIZEN)
#include "chromium_impl/content/common/paths_efl.h"
#endif

namespace mojo {

namespace {

constexpr char kCastanetsCertFileName[] = "castanets_cert.pem";
constexpr char kCastanetsKeyFileName[] = "castanets_key.pem";

base::FilePath GetCertFileDirectory() {
  base::FilePath path;
#if defined(OS_TIZEN)
  base::PathService::Get(PathsEfl::DIR_USER_DATA, &path);
#else
  base::PathService::Get(base::DIR_TEMP, &path);
#endif
  return path;
}

RSA* RSA_generate_key(int bits,
                      unsigned long e_value,
                      void* callback,
                      void* cb_arg) {
  assert(callback == NULL);
  assert(cb_arg == NULL);

  RSA* rsa = RSA_new();
  BIGNUM* e = BN_new();

  if (rsa == NULL || e == NULL || !BN_set_word(e, e_value) ||
      !RSA_generate_key_ex(rsa, bits, e, NULL)) {
    BN_free(e);
    RSA_free(rsa);
    return NULL;
  }

  BN_free(e);
  return rsa;
}

// Generates a 2048-bit RSA key.
EVP_PKEY* generate_key() {
  // Allocate memory for the EVP_PKEY structure.
  EVP_PKEY* pkey = EVP_PKEY_new();
  if (!pkey) {
    LOG(ERROR) << "Unable to create EVP_PKEY structure.";
    return NULL;
  }

  // Generate the RSA key and assign it to pkey.
  RSA* rsa = RSA_generate_key(2048, RSA_F4, NULL, NULL);
  if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
    LOG(ERROR) << "Unable to generate 2048-bit RSA key.";
    EVP_PKEY_free(pkey);
    return NULL;
  }
  return pkey;
}

// Generates a self-signed x509 certificate.
X509* generate_x509(EVP_PKEY* pkey) {
  // Allocate memory for the X509 structure.
  X509* x509 = X509_new();
  if (!x509) {
    LOG(ERROR) << "Unable to create X509 structure.";
    return NULL;
  }

  // Set the serial number.
  ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

  // This certificate is valid from now until exactly one year from now.
  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);

  // Set the public key for our certificate.
  X509_set_pubkey(x509, pkey);

  // We want to copy the subject name to the issuer name.
  X509_NAME* name = X509_get_subject_name(x509);

  // Now set the issuer name.
  X509_set_issuer_name(x509, name);

  // Actually sign the certificate with our key.
  if (!X509_sign(x509, pkey, EVP_sha1())) {
    LOG(ERROR) << "Error signing certificate.";
    X509_free(x509);
    return NULL;
  }
  return x509;
}

bool write_to_disk(EVP_PKEY* pkey, X509* x509) {
  base::FilePath dir = GetCertFileDirectory();
  // Open the PEM file for writing the key to disk.
  FILE* pkey_file =
      fopen(dir.Append(kCastanetsKeyFileName).value().c_str(), "wb");

  if (!pkey_file) {
    LOG(ERROR) << "Unable to open "
               << dir.Append(kCastanetsKeyFileName).value().c_str();
    return false;
  }

  // Write the key to disk.
  bool ret = PEM_write_PrivateKey(pkey_file, pkey, NULL, NULL, 0, NULL, NULL);
  fclose(pkey_file);

  if (!ret) {
    LOG(ERROR) << "Unable to write private key to disk.";
    return false;
  }

  // Open the PEM file for writing the certificate to disk.
  FILE* x509_file =
      fopen(dir.Append(kCastanetsCertFileName).value().c_str(), "wb");
  if (!x509_file) {
    LOG(ERROR) << "Unable to open "
               << dir.Append(kCastanetsCertFileName).value().c_str();
    return false;
  }

  // Write the certificate to disk.
  ret = PEM_write_X509(x509_file, x509);
  fclose(x509_file);

  if (!ret) {
    LOG(ERROR) << "Unable to write certificate to disk.";
    return false;
  }
  return true;
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
    EVP_PKEY* pkey = generate_key();
    CHECK(pkey);

    X509* x509 = generate_x509(pkey);
    CHECK(x509);

    bool ret = write_to_disk(pkey, x509);
    EVP_PKEY_free(pkey);
    X509_free(x509);
    CHECK(ret);

    crypto::EnsureOpenSSLInit();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx_.reset(SSL_CTX_new(TLS_server_method()));
    CHECK(ssl_ctx_);

    CHECK_GT(SSL_CTX_use_certificate_file(ssl_ctx_.get(),
                                          GetCertFileDirectory()
                                              .Append(kCastanetsCertFileName)
                                              .value()
                                              .c_str(),
                                          SSL_FILETYPE_PEM),
             0);

    CHECK_GT(SSL_CTX_use_PrivateKey_file(ssl_ctx_.get(),
                                         GetCertFileDirectory()
                                             .Append(kCastanetsKeyFileName)
                                             .value()
                                             .c_str(),
                                         SSL_FILETYPE_PEM),
             0);

    CHECK(SSL_CTX_check_private_key(ssl_ctx_.get()));

    unlink(
        GetCertFileDirectory().Append(kCastanetsKeyFileName).value().c_str());
    unlink(
        GetCertFileDirectory().Append(kCastanetsCertFileName).value().c_str());
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
