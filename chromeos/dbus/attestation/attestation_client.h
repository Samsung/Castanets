// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ATTESTATION_ATTESTATION_CLIENT_H_
#define CHROMEOS_DBUS_ATTESTATION_ATTESTATION_CLIENT_H_

#include <deque>

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/attestation/interface.pb.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// AttestationClient is used to communicate with the org.chromium.Attestation
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ATTESTATION) AttestationClient {
 public:
  using GetKeyInfoCallback =
      base::OnceCallback<void(const ::attestation::GetKeyInfoReply&)>;
  using GetEndorsementInfoCallback =
      base::OnceCallback<void(const ::attestation::GetEndorsementInfoReply&)>;
  using GetAttestationKeyInfoCallback = base::OnceCallback<void(
      const ::attestation::GetAttestationKeyInfoReply&)>;
  using ActivateAttestationKeyCallback = base::OnceCallback<void(
      const ::attestation::ActivateAttestationKeyReply&)>;
  using CreateCertifiableKeyCallback =
      base::OnceCallback<void(const ::attestation::CreateCertifiableKeyReply&)>;
  using DecryptCallback =
      base::OnceCallback<void(const ::attestation::DecryptReply&)>;
  using SignCallback =
      base::OnceCallback<void(const ::attestation::SignReply&)>;
  using RegisterKeyWithChapsTokenCallback = base::OnceCallback<void(
      const ::attestation::RegisterKeyWithChapsTokenReply&)>;
  using GetEnrollmentPreparationsCallback = base::OnceCallback<void(
      const ::attestation::GetEnrollmentPreparationsReply&)>;
  using GetStatusCallback =
      base::OnceCallback<void(const ::attestation::GetStatusReply&)>;
  using VerifyCallback =
      base::OnceCallback<void(const ::attestation::VerifyReply&)>;
  using CreateEnrollRequestCallback =
      base::OnceCallback<void(const ::attestation::CreateEnrollRequestReply&)>;
  using FinishEnrollCallback =
      base::OnceCallback<void(const ::attestation::FinishEnrollReply&)>;
  using CreateCertificateRequestCallback = base::OnceCallback<void(
      const ::attestation::CreateCertificateRequestReply&)>;
  using FinishCertificateRequestCallback = base::OnceCallback<void(
      const ::attestation::FinishCertificateRequestReply&)>;
  using EnrollCallback =
      base::OnceCallback<void(const ::attestation::EnrollReply&)>;
  using GetCertificateCallback =
      base::OnceCallback<void(const ::attestation::GetCertificateReply&)>;
  using SignEnterpriseChallengeCallback = base::OnceCallback<void(
      const ::attestation::SignEnterpriseChallengeReply&)>;
  using SignSimpleChallengeCallback =
      base::OnceCallback<void(const ::attestation::SignSimpleChallengeReply&)>;
  using SetKeyPayloadCallback =
      base::OnceCallback<void(const ::attestation::SetKeyPayloadReply&)>;
  using DeleteKeysCallback =
      base::OnceCallback<void(const ::attestation::DeleteKeysReply&)>;
  using ResetIdentityCallback =
      base::OnceCallback<void(const ::attestation::ResetIdentityReply&)>;
  using GetEnrollmentIdCallback =
      base::OnceCallback<void(const ::attestation::GetEnrollmentIdReply&)>;
  using GetCertifiedNvIndexCallback =
      base::OnceCallback<void(const ::attestation::GetCertifiedNvIndexReply&)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Sets the preparation status to |is_prepared|. If no injected sequence by
    // |ConfigureEnrollmentPreparationsSequence| the enrollment preparations
    // always returns |is_prepared|.
    virtual void ConfigureEnrollmentPreparations(bool is_prepared) = 0;
    // Injects |sequence| of enrollment preparations. Once injected, the
    // returned enrollment preparations status will be the element popped from
    // the |sequence| one-by-one until all the elements are consumed.
    virtual void ConfigureEnrollmentPreparationsSequence(
        std::deque<bool> sequence) = 0;
  };

  // Not copyable or movable.
  AttestationClient(const AttestationClient&) = delete;
  AttestationClient& operator=(const AttestationClient&) = delete;
  AttestationClient(AttestationClient&&) = delete;
  AttestationClient& operator=(AttestationClient&&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static AttestationClient* Get();

  // Attestation daemon D-Bus method calls. See org.chromium.Attestation.xml and
  // the corresponding protobuf definitions in Chromium OS code for the
  // documentation of the methods and request/ messages.

  virtual void GetKeyInfo(const ::attestation::GetKeyInfoRequest& request,
                          GetKeyInfoCallback callback) = 0;

  virtual void GetEndorsementInfo(
      const ::attestation::GetEndorsementInfoRequest& request,
      GetEndorsementInfoCallback callback) = 0;

  virtual void GetAttestationKeyInfo(
      const ::attestation::GetAttestationKeyInfoRequest& request,
      GetAttestationKeyInfoCallback callback) = 0;

  virtual void ActivateAttestationKey(
      const ::attestation::ActivateAttestationKeyRequest& request,
      ActivateAttestationKeyCallback callback) = 0;

  virtual void CreateCertifiableKey(
      const ::attestation::CreateCertifiableKeyRequest& request,
      CreateCertifiableKeyCallback callback) = 0;

  virtual void Decrypt(const ::attestation::DecryptRequest& request,
                       DecryptCallback callback) = 0;

  virtual void Sign(const ::attestation::SignRequest& request,
                    SignCallback callback) = 0;

  virtual void RegisterKeyWithChapsToken(
      const ::attestation::RegisterKeyWithChapsTokenRequest& request,
      RegisterKeyWithChapsTokenCallback callback) = 0;

  virtual void GetEnrollmentPreparations(
      const ::attestation::GetEnrollmentPreparationsRequest& request,
      GetEnrollmentPreparationsCallback callback) = 0;

  virtual void GetStatus(const ::attestation::GetStatusRequest& request,
                         GetStatusCallback callback) = 0;

  virtual void Verify(const ::attestation::VerifyRequest& request,
                      VerifyCallback callback) = 0;

  virtual void CreateEnrollRequest(
      const ::attestation::CreateEnrollRequestRequest& request,
      CreateEnrollRequestCallback callback) = 0;

  virtual void FinishEnroll(const ::attestation::FinishEnrollRequest& request,
                            FinishEnrollCallback callback) = 0;

  virtual void CreateCertificateRequest(
      const ::attestation::CreateCertificateRequestRequest& request,
      CreateCertificateRequestCallback callback) = 0;

  virtual void FinishCertificateRequest(
      const ::attestation::FinishCertificateRequestRequest& request,
      FinishCertificateRequestCallback callback) = 0;

  virtual void Enroll(const ::attestation::EnrollRequest& request,
                      EnrollCallback callback) = 0;

  virtual void GetCertificate(
      const ::attestation::GetCertificateRequest& request,
      GetCertificateCallback callback) = 0;

  virtual void SignEnterpriseChallenge(
      const ::attestation::SignEnterpriseChallengeRequest& request,
      SignEnterpriseChallengeCallback callback) = 0;

  virtual void SignSimpleChallenge(
      const ::attestation::SignSimpleChallengeRequest& request,
      SignSimpleChallengeCallback callback) = 0;

  virtual void SetKeyPayload(const ::attestation::SetKeyPayloadRequest& request,
                             SetKeyPayloadCallback callback) = 0;

  virtual void DeleteKeys(const ::attestation::DeleteKeysRequest& request,
                          DeleteKeysCallback callback) = 0;

  virtual void ResetIdentity(const ::attestation::ResetIdentityRequest& request,
                             ResetIdentityCallback callback) = 0;

  virtual void GetEnrollmentId(
      const ::attestation::GetEnrollmentIdRequest& request,
      GetEnrollmentIdCallback callback) = 0;

  virtual void GetCertifiedNvIndex(
      const ::attestation::GetCertifiedNvIndexRequest& request,
      GetCertifiedNvIndexCallback callback) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  AttestationClient();
  virtual ~AttestationClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ATTESTATION_ATTESTATION_CLIENT_H_
