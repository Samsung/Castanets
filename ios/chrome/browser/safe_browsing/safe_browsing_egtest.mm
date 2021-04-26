// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

namespace {

// Text that is found when expanding details on the phishing warning page.
const char kPhishingWarningDetails[] =
    "Google Safe Browsing recently detected phishing";

// Text that is found when expanding details on the malware warning page.
const char kMalwareWarningDetails[] =
    "Google Safe Browsing recently detected malware";

}

// Tests Safe Browsing URL blocking.
@interface SafeBrowsingTestCase : ChromeTestCase {
  // A URL that is treated as an unsafe phishing page.
  GURL _phishingURL;
  // Text that is found on the phishing page.
  std::string _phishingContent;
  // A URL that is treated as an unsafe malware page.
  GURL _malwareURL;
  // Text that is found on the malware page.
  std::string _malwareContent;
  // A URL of a page with an iframe that is treated as having malware.
  GURL _iframeWithMalwareURL;
  // Text that is found on the iframe that is treated as having malware.
  std::string _iframeWithMalwareContent;
  // A URL that is treated as a safe page.
  GURL _safeURL1;
  // Text that is found on the safe page.
  std::string _safeContent1;
  // Another URL that is treated as a safe page.
  GURL _safeURL2;
  // Text that is found on the safe page.
  std::string _safeContent2;
}
@end

@implementation SafeBrowsingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(safe_browsing::kSafeBrowsingAvailableOnIOS);
  config.features_enabled.push_back(web::features::kSSLCommittedInterstitials);

  // Use commandline args to insert fake unsafe URLs into the Safe Browsing
  // database.
  config.additional_args.push_back(std::string("--mark_as_phishing=") +
                                   _phishingURL.spec());
  config.additional_args.push_back(std::string("--mark_as_malware=") +
                                   _malwareURL.spec());
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  bool started = self.testServer->Start();
  _phishingURL = self.testServer->GetURL("/set-invalid-cookie");
  _phishingContent = "TEST";

  _malwareURL = self.testServer->GetURL("/defaultresponse");
  _malwareContent = "Default";

  _iframeWithMalwareURL =
      self.testServer->GetURL("/iframe?" + _malwareURL.spec());
  _iframeWithMalwareContent = _malwareContent;

  _safeURL1 = self.testServer->GetURL("/echo");
  _safeContent1 = "Echo";

  _safeURL2 = self.testServer->GetURL("/echoall");
  _safeContent2 = "Request Body";

  // |appConfigurationForTestCase| is called during [super setUp], and
  // depends on the URLs initialized above.
  [super setUp];

  // GREYAssertTrue cannot be called before [super setUp].
  GREYAssertTrue(started, @"Test server failed to start.");

  // Ensure that Safe Browsing opt-out starts in its default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
}

// Tests that safe pages are not blocked.
- (void)testSafePage {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
}

// Tests that a phishing page is blocked, and the "Back to safety" button on
// the warning page works as expected.
- (void)testPhishingPage {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the "Back to safety" button and verify that the previous page's
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
}

// Tests expanding the details on a phishing warning, and proceeding past the
// warning. Also verifies that a warning is still shown when visiting the unsafe
// URL in a new tab.
- (void)testProceedingPastPhishingWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kPhishingWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];

  // In a new tab, a warning should still be shown, even though the user
  // proceeded to the unsafe content in the other tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

// Tests expanding the details on a phishing warning, and proceeding past the
// warning in incognito mode. Also verifies that a warning is still shown when
// visiting the unsafe URL in a new incognito tab.
- (void)testProceedingPastPhishingWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kPhishingWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];

  // In a new tab, a warning should still be shown, even though the user
  // proceeded to the unsafe content in the other tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];
}

// Tests that a malware page is blocked, and the "Back to safety" button on the
// warning page works as expected.
- (void)testMalwarePage {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the "Back to safety" button and verify that the previous page's
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
}

// Tests expanding the details on a malware warning, proceeding past the
// warning, and navigating back/forward to the unsafe page.
- (void)testProceedingPastMalwareWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kMalwareWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];

  // Visit another safe page, and then navigate back to the unsafe page and
  // verify that no warning is shown.
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];
}

// Tests expanding the details on a malware warning, proceeding past the
// warning, and navigating back/forward to the unsafe page, in incognito mode.
- (void)testProceedingPastMalwareWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kMalwareWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];

  // Visit another safe page, and then navigate back to the unsafe page and
  // verify that no warning is shown.
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];
}

// Tests that disabling and re-enabling Safe Browsing works as expected.
- (void)testDisableAndEnableSafeBrowsing {
  // Disable Safe Browsing and verify that unsafe content is shown.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];

  // Re-enable Safe Browsing and verify that a warning is shown for unsafe
  // content.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests displaying a warning for an unsafe page in incognito mode, and
// proceeding past the warning.
- (void)testWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the phishing page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_PHISHING_V4_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kPhishingWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];
}

// Tests that the proceed option is not shown when
// kSafeBrowsingProceedAnywayDisabled is enabled.
- (void)testProceedAlwaysDisabled {
  // Enable the pref.
  NSString* prefName =
      base::SysUTF8ToNSString(prefs::kSafeBrowsingProceedAnywayDisabled);
  [ChromeEarlGreyAppInterface setBoolValue:YES forUserPref:prefName];

  // Load the a malware safe browsing error page.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains malware"];

  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:
                      "Google Safe Browsing recently detected malware"];

  // Verify that the proceed-link element is not found.  When the proceed link
  // is disabled, the entire second paragraph hidden.
  NSString* selector =
      @"(function() {"
       "  var element = document.getElementById('final-paragraph');"
       "  if (element.classList.contains('hidden')) return true;"
       "  return false;"
       "})()";
  NSString* description = @"Hidden proceed-anyway link.";
  ElementSelector* proceedLink =
      [ElementSelector selectorWithScript:selector
                      selectorDescription:description];
  GREYAssert(
      [ChromeEarlGreyAppInterface webStateContainsElement:proceedLink],
      @"Proceed anyway link shown despite kSafeBrowsingProceedAnywayDisabled");
}

// Tests performing a back navigation to a warning page and a forward navigation
// from a warning page.
- (void)testBackForwardNavigationWithWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests performing a back navigation to a warning page and a forward navigation
// from a warning page, in incognito mode.
- (void)testBackForwardNavigationWithWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests that performing session restoration to a Safe Browsing warning page
// preserves navigation history.
- (void)testRestoreToWarningPagePreservesHistory {
  // Build up navigation history that consists of a safe URL, a warning page,
  // and another safe URL.
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load the malware page and verify a warning is shown.
  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the "Back to safety" button and verify that the previous page's
  // contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  // Navigate back so that both the back list and the forward list are
  // non-empty.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Do a session restoration and verify that all navigation history is
  // preserved.
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests that a page with an unsafe ifame is blocked, back history is preserved,
// and forward navigation to the warning works as expected.
- (void)testPageWithUnsafeIframe {
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Ensure back history is preserved. Tap on the "Back to safety" button and
  // verify that the previous page's contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  // Verify that going forward results in the warning being displayed.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests that a page with an unsafe ifame is blocked, back history is preserved,
// and forward navigation to the warning works as expected, in incognito mode.
- (void)testPageWithUnsafeIframeInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Ensure back history is preserved. Tap on the "Back to safety" button and
  // verify that the previous page's contents are loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  // Verify that going forward results in the warning being displayed.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

// Tests performing a back navigation to a warning page for an unsafe iframe,
// and then performing a forward navigation from the warning.
- (void)testBackForwardNavigationWithIframeWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
}

// Tests expanding the details on a warning for an unsafe iframe, proceeding
// past the warning, and navigating away from and back to the unsafe page. Also
// verifies that a warning is still shown when visiting the unsafe URL in a new
// tab.
- (void)testProceedingPastIframeWarning {
  [ChromeEarlGrey loadURL:_safeURL1];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];

  // Load a page that has an iframe with malware, and verify that a warning is
  // shown.
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];

  // Tap on the Details button and verify that warning details are shown.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:kMalwareWarningDetails];

  // Tap on the link to proceed to the unsafe page, and verify that this page is
  // loaded.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that no warning is shown when navigating back and then forward to
  // the unsafe page.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent1];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Visit another safe page, and then navigate back to the unsafe page and
  // verify that no warning is shown.
  [ChromeEarlGrey loadURL:_safeURL2];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateFrameContainingText:_malwareContent];

  // Verify that a warning is still shown when loading the page in a new tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_iframeWithMalwareURL];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_MALWARE_V3_HEADING)];
}

@end
