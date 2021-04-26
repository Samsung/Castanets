// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"

#include <cstdint>
#include <vector>

#include "base/strings/string_piece_forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_Basic) {
  const uint8_t kInput[] = {1, 2, 3, 4, 5};
  auto digest = IdentifiabilityDigestOfBytes(kInput);

  // Due to our requirement that the digest be stable and persistable, this test
  // should always pass once the code reaches the stable branch.
  EXPECT_EQ(UINT64_C(0x7cd845f1db5ad659), digest);
}

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_Padding) {
  const uint8_t kTwoBytes[] = {1, 2};
  const std::vector<uint8_t> kLong(16 * 1024, 'x');

  // Ideally we should be using all 64-bits or at least the 56 LSBs.
  EXPECT_EQ(UINT64_C(0xb74c74c9fcf0505a),
            IdentifiabilityDigestOfBytes(kTwoBytes));
  EXPECT_EQ(UINT64_C(0x76b3567105dc5253), IdentifiabilityDigestOfBytes(kLong));
}

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_EdgeCases) {
  const std::vector<uint8_t> kEmpty;
  const uint8_t kOneByte[] = {1};

  // As before, these tests should always pass.
  EXPECT_EQ(UINT64_C(0x9ae16a3b2f90404f), IdentifiabilityDigestOfBytes(kEmpty));
  EXPECT_EQ(UINT64_C(0x6209312a69a56947),
            IdentifiabilityDigestOfBytes(kOneByte));
}

TEST(IdentifiabilityMetricsTest, PassInt) {
  EXPECT_EQ(UINT64_C(5), IdentifiabilityDigestHelper(5));
}

TEST(IdentifiabilityMetricsTest, PassChar) {
  EXPECT_EQ(UINT64_C(97), IdentifiabilityDigestHelper('a'));
}

TEST(IdentifiabilityMetricsTest, PassBool) {
  EXPECT_EQ(UINT64_C(1), IdentifiabilityDigestHelper(true));
}

TEST(IdentifiabilityMetricsTest, PassLong) {
  EXPECT_EQ(UINT64_C(5), IdentifiabilityDigestHelper(5L));
}

TEST(IdentifiabilityMetricsTest, PassUint16) {
  EXPECT_EQ(UINT64_C(5), IdentifiabilityDigestHelper(static_cast<uint16_t>(5)));
}

TEST(IdentifiabilityMetricsTest, PassSizeT) {
  EXPECT_EQ(UINT64_C(1), IdentifiabilityDigestHelper(sizeof(char)));
}

TEST(IdentifiabilityMetricsTest, PassFloat) {
  EXPECT_NE(UINT64_C(0), IdentifiabilityDigestHelper(5.0f));
}

TEST(IdentifiabilityMetricsTest, PassDouble) {
  EXPECT_NE(UINT64_C(0), IdentifiabilityDigestHelper(5.0));
}

// Use an arbitrary, large number to make accidental matches unlikely.
enum SimpleEnum { kSimpleValue = 2730421 };

TEST(IdentifiabilityMetricsTest, PassEnum) {
  EXPECT_EQ(UINT64_C(2730421), IdentifiabilityDigestHelper(kSimpleValue));
}

namespace {

// Use an arbitrary, large number to make accidental matches unlikely.
enum Simple64Enum : uint64_t { kSimple64Value = 4983422 };

// Use an arbitrary, large number to make accidental matches unlikely.
enum class SimpleEnumClass { kSimpleValue = 3498249 };

// Use an arbitrary, large number to make accidental matches unlikely.
enum class SimpleEnumClass64 : uint64_t { kSimple64Value = 4398372 };

constexpr uint64_t kExpectedCombinationResult = UINT64_C(0xa5e30a57547cd49b);

}  // namespace

TEST(IdentifiabilityMetricsTest, PassEnum64) {
  EXPECT_EQ(UINT64_C(4983422), IdentifiabilityDigestHelper(kSimple64Value));
}

TEST(IdentifiabilityMetricsTest, PassEnumClass) {
  EXPECT_EQ(UINT64_C(3498249),
            IdentifiabilityDigestHelper(SimpleEnumClass::kSimpleValue));
}

TEST(IdentifiabilityMetricsTest, PassEnumClass64) {
  EXPECT_EQ(UINT64_C(4398372),
            IdentifiabilityDigestHelper(SimpleEnumClass64::kSimple64Value));
}

TEST(IdentifiabilityMetricsTest, PassSpan) {
  const int data[] = {1, 2, 3};
  EXPECT_EQ(UINT64_C(0xb0dd8c7041b0a8bb),
            IdentifiabilityDigestHelper(base::make_span(data)));
}

TEST(IdentifiabilityMetricsTest, PassSpanDouble) {
  const double data[] = {1.0, 2.0, 3.0};
  EXPECT_EQ(UINT64_C(0x95f52e9784f9582a),
            IdentifiabilityDigestHelper(base::make_span(data)));
}

TEST(IdentifiabilityMetricsTest, Combination) {
  const int data[] = {1, 2, 3};
  EXPECT_EQ(kExpectedCombinationResult,
            IdentifiabilityDigestHelper(
                5, 'a', true, static_cast<uint16_t>(5), sizeof(char),
                kSimpleValue, kSimple64Value, SimpleEnumClass::kSimpleValue,
                SimpleEnumClass64::kSimple64Value, base::make_span(data)));
}

TEST(IdentifiabilityMetricsTest, CombinationWithFloats) {
  const int data[] = {1, 2, 3};
  const int data_doubles[] = {1.0, 2.0, 3.0};
  EXPECT_NE(kExpectedCombinationResult,
            IdentifiabilityDigestHelper(
                5, 'a', true, static_cast<uint16_t>(5), sizeof(char),
                kSimpleValue, kSimple64Value, SimpleEnumClass::kSimpleValue,
                SimpleEnumClass64::kSimple64Value, 5.0f, 5.0,
                base::make_span(data), base::make_span(data_doubles)));
}

}  // namespace blink
