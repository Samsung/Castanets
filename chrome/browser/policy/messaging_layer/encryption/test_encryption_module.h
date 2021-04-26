// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_

#include <string>

#include "chrome/browser/policy/messaging_layer/public/report_queue.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

// An |EncryptionModule| that does no encryption.
class TestEncryptionModule : public EncryptionModule {
 public:
  TestEncryptionModule() = default;

  StatusOr<std::string> EncryptRecord(base::StringPiece record) const override;

 protected:
  ~TestEncryptionModule() override = default;
};

// A |TestEncryptionModule| that always fails on |EncryptRecord| calls.
class AlwaysFailsEncryptionModule final : public TestEncryptionModule {
 public:
  AlwaysFailsEncryptionModule() = default;

  StatusOr<std::string> EncryptRecord(base::StringPiece record) const override;

 protected:
  ~AlwaysFailsEncryptionModule() override = default;
};

}  // namespace test
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_TEST_ENCRYPTION_MODULE_H_
