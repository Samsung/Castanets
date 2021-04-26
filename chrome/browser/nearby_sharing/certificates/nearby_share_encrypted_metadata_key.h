// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_ENCRYPTED_METADATA_KEY_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_ENCRYPTED_METADATA_KEY_H_

#include <vector>

// Holds the encrypted symmetric key--the key used to encrypt user/device
// metatdata--as well as the salt used to encrypt the key.
struct NearbyShareEncryptedMetadataKey {
 public:
  NearbyShareEncryptedMetadataKey(std::vector<uint8_t> encrypted_key,
                                  std::vector<uint8_t> salt);
  NearbyShareEncryptedMetadataKey(NearbyShareEncryptedMetadataKey&&);
  ~NearbyShareEncryptedMetadataKey();

  const std::vector<uint8_t>& encrypted_key() const { return encrypted_key_; }
  const std::vector<uint8_t>& salt() const { return salt_; }

 private:
  std::vector<uint8_t> encrypted_key_;
  std::vector<uint8_t> salt_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_ENCRYPTED_METADATA_KEY_H_
