// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class TelemetryExtensionIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  TelemetryExtensionIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kTelemetryExtension}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Telemetry Extension installs and launches correctly. Runs
// some spot checks on the manifest.
#if defined(ADDRESS_SANITIZER)
// Flaky under asan/lsan: https://crbug.com/1098764
#define MAYBE_TelemetryExtension DISABLED_TelemetryExtension
#else
#define MAYBE_TelemetryExtension TelemetryExtension
#endif
IN_PROC_BROWSER_TEST_P(TelemetryExtensionIntegrationTest,
                       MAYBE_TelemetryExtension) {
  const GURL url(chromeos::kChromeUITelemetryExtensionURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::TELEMETRY, url, "Telemetry Extension"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         TelemetryExtensionIntegrationTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
