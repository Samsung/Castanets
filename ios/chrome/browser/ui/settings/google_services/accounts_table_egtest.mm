// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils_app_interface.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SignOutAccountsButton;
using chrome_test_util::PrimarySignInButton;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 10.0;

// Returns a matcher for a button that matches the userEmail in the given
// |fakeIdentity|.
id<GREYMatcher> ButtonWithFakeIdentity(FakeChromeIdentity* fakeIdentity) {
  return ButtonWithAccessibilityLabel(fakeIdentity.userEmail);
}

// Returns a matcher for when there are no bookmarks saved.
id<GREYMatcher> NoBookmarksLabel() {
  return grey_text(l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL));
}
}

// Integration tests using the Account Settings screen.
@interface AccountCollectionsTestCase : ChromeTestCase
@end

@implementation AccountCollectionsTestCase

- (void)tearDown {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];

  [ChromeEarlGrey clearSyncServerData];
  [super tearDown];
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::BOOKMARKS], 0,
      @"No bookmarks should exist before tests start.");
}

// Tests that the Sync and Account Settings screen are correctly popped if the
// signed in account is removed.
- (void)testSignInPopUpAccountOnSyncSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |identity|, then open the Sync Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Forget |fakeIdentity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [SigninEarlGreyUtils forgetFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils checkSignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];

  // Forget |fakeIdentity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [SigninEarlGreyUtils forgetFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils checkSignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testSignInReloadOnRemoveAccount {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGreyUtils fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGreyUtils fakeIdentity2];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity2];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |fakeIdentity2| from the device.
  [[EarlGrey selectElementWithMatcher:ButtonWithFakeIdentity(fakeIdentity2)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  // Check that |fakeIdentity2| isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity2.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity1];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is popped and the user signed out
// when the account is removed.
- (void)testSignOutOnRemoveAccount {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |fakeIdentity| from the device.
  [[EarlGrey selectElementWithMatcher:ButtonWithFakeIdentity(fakeIdentity)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils checkSignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that selecting sign-out from a non-managed account keeps the user's
// synced data.
- (void)testSignOutFromNonManagedAccountKeepsData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [SigninEarlGreyUtilsAppInterface addBookmark:@"http://youtube.com"
                                     withTitle:@"cats"];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithSignOutConfirmation:SignOutConfirmationNonManagedUser];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the 'cats' bookmark is displayed.
  [[EarlGrey selectElementWithMatcher:grey_text(@"cats")]
      assertWithMatcher:grey_notNil()];
}

// Tests that selecting sign-out and clear data from a non-managed user account
// clears the user's synced data.
- (void)testSignOutAndClearDataFromNonManagedAccountClearsData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [SigninEarlGreyUtilsAppInterface addBookmark:@"http://youtube.com"
                                     withTitle:@"cats"];

  // Sign out.
  [SigninEarlGreyUI signOutWithSignOutConfirmation:
                        SignOutConfirmationNonManagedUserWithClearedData];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that there are no bookmarks.
  [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
      assertWithMatcher:grey_notNil()];
}

// Tests that signing out from a managed user account clears the user's data.
- (void)testsSignOutFromManagedAccount {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeManagedIdentity];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity isManagedAccount:YES];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [SigninEarlGreyUtilsAppInterface addBookmark:@"http://youtube.com"
                                     withTitle:@"cats"];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithSignOutConfirmation:SignOutConfirmationManagedUser];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that there are no bookmarks.
  [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
      assertWithMatcher:grey_notNil()];
}

// Tests that the user isn't signed out and the UI is correct when the
// disconnect is cancelled in the Account Settings screen.
#if !TARGET_IPHONE_SIMULATOR
// TODO(crbug.com/669613): Re-enable this test on devices.
#define MAYBE_testSignInDisconnectCancelled \
  DISABLED_testSignInDisconnectCancelled
#else
#define MAYBE_testSignInDisconnectCancelled testSignInDisconnectCancelled
#endif
- (void)MAYBE_testSignInDisconnectCancelled {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open the "Disconnect Account" dialog, then tap "Cancel".
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
  // Note that the iPad does not provide a CANCEL button by design. Click
  // anywhere on the screen to exit.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Check that Account Settings screen is open and |fakeIdentity| is signed in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsAccountsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
