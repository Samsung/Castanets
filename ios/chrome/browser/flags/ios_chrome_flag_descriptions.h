// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

#include "Availability.h"

// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

// Title and description for the flag to add a custom drop interaction to web
// content.
extern const char kAddWebContentDropInteractionName[];
extern const char kAddWebContentDropInteractionDescription[];

// Title and description for the flag to control the autofill query cache.
extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to control card nickname management.
extern const char kAutofillEnableCardNicknameManagementName[];
extern const char kAutofillEnableCardNicknameManagementDescription[];

// Title and description for the flag to control card nickname upstream.
extern const char kAutofillEnableCardNicknameUpstreamName[];
extern const char kAutofillEnableCardNicknameUpstreamDescription[];

// Title and description for the flag to control deprecating company name.
extern const char kAutofillEnableCompanyNameName[];
extern const char kAutofillEnableCompanyNameDescription[];

// Title and description for the flag to control enabling Google-issued cards in
// autofill suggestions.
extern const char kAutofillEnableGoogleIssuedCardName[];
extern const char kAutofillEnableGoogleIssuedCardDescription[];

// Title and description for the flag to control offers in downstream.
extern const char kAutofillEnableOffersInDownstreamName[];
extern const char kAutofillEnableOffersInDownstreamDescription[];

// Title and description for the flag to control surfacing server card nickname.
extern const char kAutofillEnableSurfacingServerCardNicknameName[];
extern const char kAutofillEnableSurfacingServerCardNicknameDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag that controls whether the maximum number
// of Autofill suggestions shown is pruned.
extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

// Title and description for the flag to control dismissing the Save Card
// Infobar on Navigation.
extern const char kAutofillSaveCardDismissOnNavigationName[];
extern const char kAutofillSaveCardDismissOnNavigationDescription[];

// Title and description for the flag that enables editing on the Messages UI
// for SaveCard Infobars.
extern const char kAutofillSaveCardInfobarEditSupportName[];
extern const char kAutofillSaveCardInfobarEditSupportDescription[];

// Title and description for the flag to control if prefilled value filter
// profiles.
extern const char kAutofillShowAllSuggestionsOnPrefilledFormsName[];
extern const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[];

// Title and description for the flag to restrict extraction of formless forms
// to checkout flows.
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

// Title and description for the flag to enable rich autofill queries on
// Canary/Dev.
extern const char kAutofillRichMetadataQueriesName[];
extern const char kAutofillRichMetadataQueriesDescription[];

// Title and description for the flag that controls whether Autofill's
// suggestions' labels are formatting with a mobile-friendly approach.
extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

// Title and description for the flag that enables Messages UI on
// Block Popup Infobars.
extern const char kBlockPopupInfobarMessagesUIName[];
extern const char kBlockPopupInfobarMessagesUIDescription[];

// Title and description for the flag that controls whether event breadcrumbs
// are captured.
extern const char kLogBreadcrumbsName[];
extern const char kLogBreadcrumbsDescription[];

// Title and description for the flag to control if initial uploading of crash
// reports is delayed.
extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

// Title and description for the flag that controls the tab switcher position.
extern const char kChangeTabSwitcherPositionName[];
extern const char kChangeTabSwitcherPositionDescription[];

// Title and description for the flag that controls whether Collections are
// presented using the new iOS13 Card style or the custom legacy one.
extern const char kCollectionsCardPresentationStyleName[];
extern const char kCollectionsCardPresentationStyleDescription[];

// Title and description for the flag that enables Messages UI on
// ConfirmInfobars.
extern const char kConfirmInfobarMessagesUIName[];
extern const char kConfirmInfobarMessagesUIDescription[];

// Title and description for the flag that makes the Browser being contained by
// the TabGrid instead of being presented.
extern const char kContainedBVCName[];
extern const char kContainedBVCDescription[];

// Title and description for the flag that enables Messages UI on
// Crash Restore Infobars.
extern const char kCrashRestoreInfobarMessagesUIName[];
extern const char kCrashRestoreInfobarMessagesUIDescription[];

// Title and description for the flag to scan a new credit card using the
// camera.
extern const char kCreditCardScannerName[];
extern const char kCreditCardScannerDescription[];

#if defined(DCHECK_IS_CONFIGURABLE)
// Title and description for the flag to enable configurable DCHECKs.
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// Title and description for the flag to request the desktop version of web site
// by default on iPad
extern const char kDefaultToDesktopOnIPadName[];
extern const char kDefaultToDesktopOnIPadDescription[];

// Title and description for the flag to control if a crash report is generated
// on main thread freeze.
extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

// Title and description for the flag to disable progress bar animation.
extern const char kDisableProgressBarAnimationName[];
extern const char kDisableProgressBarAnimationDescription[];

// Title and description for the flag to replace the Zine feed with the
// Discover feed in the Bling NTP.
extern const char kDiscoverFeedInNtpName[];
extern const char kDiscoverFeedInNtpDescription[];

// Title and description for the flag to enable the Messages UI for downloads.
extern const char kDownloadInfobarMessagesUIName[];
extern const char kDownloadInfobarMessagesUIDescription[];

// Title and description for the flag to enable drag and drop.
extern const char kDragAndDropName[];
extern const char kDragAndDropDescription[];

// Title and description for the flag to enable EditBookmarks enterprise
// policy on iOS.
extern const char kEditBookmarksIOSName[];
extern const char kEditBookmarksIOSDescription[];

// Title and description for the flag to block restore urls.
extern const char kEmbedderBlockRestoreUrlName[];
extern const char kEmbedderBlockRestoreUrlDescription[];

// Title and description for the flag to enable caching of unmasked server
// cards until page navigation to simplify consecutive fills on the same page.
extern const char kEnableAutofillCacheServerCardInfoName[];
extern const char kEnableAutofillCacheServerCardInfoDescription[];

// Title and description for the flag to enable the clipboard provider to
// suggest searchihng for copied images.
extern const char kEnableClipboardProviderImageSuggestionsName[];
extern const char kEnableClipboardProviderImageSuggestionsDescription[];

// Title and description for the flag to enable to show a different UI when the
// setting is managed by an enterprise policy.
extern const char kEnableIOSManagedSettingsUIName[];
extern const char kEnableIOSManagedSettingsUIDescription[];

// Title and description for the flag to enable MyGoogle account management
// UI in iOS Settings.
extern const char kEnableMyGoogleName[];
extern const char kEnableMyGoogleDescription[];

// Title and description for the flag to enable new context menus for native UI.
extern const char kEnableNativeContextMenusName[];
extern const char kEnableNativeContextMenusDescription[];

// Title and description for the flag to enable persistent downloads.
extern const char kEnablePersistentDownloadsName[];
extern const char kEnablePersistentDownloadsDescription[];

extern const char kEnableSyncTrustedVaultName[];
extern const char kEnableSyncTrustedVaultDescription[];

// Title and description for the flag to enable an expanded tab strip.
extern const char kExpandedTabStripName[];
extern const char kExpandedTabStripDescription[];

// Title and description for the flag to trigger the startup sign-in promo.
extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

// Title and description for the flag to force an unstacked tabstrip.
extern const char kForceUnstackedTabstripName[];
extern const char kForceUnstackedTabstripDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

// Title and description for the flag to scope FullscreenController to a
// Browser.
extern const char kFullscreenControllerBrowserScopedName[];
extern const char kFullscreenControllerBrowserScopedDescription[];

// Title and description for the flag to enable new illustrations and
// UI on empty states.
extern const char kIllustratedEmptyStatesName[];
extern const char kIllustratedEmptyStatesDescription[];

// Title and description for the flag to present the new UI Reboot on Infobars
// using OverlayPresenter.
extern const char kInfobarOverlayUIName[];
extern const char kInfobarOverlayUIDescription[];

// Title and description for the flag to enable the new UI Reboot on Infobars.
extern const char kInfobarUIRebootName[];
extern const char kInfobarUIRebootDescription[];

// Title and description for the flag to enable the new UI Reboot on Infobars
// only on iOS13.
extern const char kInfobarUIRebootOnlyiOS13Name[];
extern const char kInfobarUIRebootOnlyiOS13Description[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable interstitials on lookalike
// URL navigations.
extern const char kIOSLookalikeUrlNavigationSuggestionsUIName[];
extern const char kIOSLookalikeUrlNavigationSuggestionsUIDescription[];

// Title and description for the flag to lock the bottom toolbar into place.
extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

// Title and description for the flag to enable ManagedBookmarks enterprise
// policy on iOS.
extern const char kManagedBookmarksIOSName[];
extern const char kManagedBookmarksIOSDescription[];

// Title, description, and options for the MarkHttpAs setting that controls
// display of omnibox warnings about non-secure pages.
extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];

// Title and description for the flag where the Google SRP is requested in
// mobile mode by default.
extern const char kMobileGoogleSRPName[];
extern const char kMobileGoogleSRPDescription[];

// Title and description for the flag to preserve the default match when an
// async match updates.
extern const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateName[];
extern const char kOmniboxPreserveDefaultMatchAgainstAsyncUpdateDescription[];

// Title and description for the flag to change the max number of autocomplete
// matches in the omnibox popup.
extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (non incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

// Title and description for the flag to control Omnibox on-focus suggestions.
extern const char kOmniboxOnFocusSuggestionsName[];
extern const char kOmniboxOnFocusSuggestionsDescription[];

// Title and description for the flag to open Downloaded files in Files.app.
extern const char kOpenDownloadsInFilesAppName[];
extern const char kOpenDownloadsInFilesAppDescription[];

// Title and description for the flag to provide user with improved settings
// relating to cookies.
extern const char kImprovedCookieControlsName[];
extern const char kImprovedCookieControlsDescription[];

// Title and description for the flag to provide user with Password Check
// feature in Settings.
extern const char kPasswordCheckName[];
extern const char kPasswordCheckDescription[];

// Title and description for the flag to enable the new design of the page info.
extern const char kPageInfoRefactoringName[];
extern const char kPageInfoRefactoringDescription[];

#if defined(__IPHONE_13_4)
// Title and description for the flag to enable pointer support on tablets.
extern const char kPointerSupportName[];
extern const char kPointerSupportDescription[];
#endif  // defined(__IPHONE_13_4)

// Title and description for the flag to enable QR code generation for a page.
extern const char kQRCodeGenerationName[];
extern const char kQRCodeGenerationDescription[];

// Title and description for the flag that reload the page when the renderer
// crashes.
extern const char kReloadSadTabName[];
extern const char kReloadSadTabDescription[];

// Title and description for the flag that makes Safe Browsing available.
extern const char kSafeBrowsingAvailableName[];
extern const char kSafeBrowsingAvailableDescription[];

// Title and description for the flag to enable safety check on iOS.
extern const char kSafetyCheckIOSName[];
extern const char kSafetyCheckIOSDescription[];

// Title and description for the flag that enables Messages UI on
// SaveCard Infobars.
extern const char kSaveCardInfobarMessagesUIName[];
extern const char kSaveCardInfobarMessagesUIDescription[];

// Title and description for the flag to enable the send tab to self receiving
// feature.
extern const char kSendTabToSelfName[];
extern const char kSendTabToSelfDescription[];

// Title and description for the flag to send UMA data over any network.
extern const char kSendUmaOverAnyNetwork[];
extern const char kSendUmaOverAnyNetworkDescription[];

// Title and description for the flag to toggle the flag for the settings UI
// Refresh.
extern const char kSettingsRefreshName[];
extern const char kSettingsRefreshDescription[];

// Title and description for the flag to enable Shared Highlighting (Link to
// Text Edit Menu option and Text Fragments web behavior).
extern const char kSharedHighlightingIOSName[];
extern const char kSharedHighlightingIOSDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to use |-drawViewHierarchy:| for taking
// snapshots.
extern const char kSnapshotDrawViewName[];
extern const char kSnapshotDrawViewDescription[];

// Title and description for the flag to enable SSL committed interstitials.
extern const char kSSLCommittedInterstitialsName[];
extern const char kSSLCommittedInterstitialsDescription[];

// Title and description for the flag to allow syncing DeviceInfo in
// transport-only mode.
extern const char kSyncDeviceInfoInTransportModeName[];
extern const char kSyncDeviceInfoInTransportModeDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable the Messages UI for Translate
// Infobars.
extern const char kTranslateInfobarMessagesUIName[];
extern const char kTranslateInfobarMessagesUIDescription[];

// Title and description for the flag to enable URLBlocklist/URLAllowlist
// enterprise policy.
extern const char kURLBlocklistIOSName[];
extern const char kURLBlocklistIOSDescription[];

// Title and description for the flag to enable the new error page workflow.
extern const char kUseJSForErrorPageName[];
extern const char kUseJSForErrorPageDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to tie the default text zoom level to
// the dynamic type setting.
extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

// Title and description for the flag to enable text accessibility in webpages.
extern const char kWebPageTextAccessibilityName[];
extern const char kWebPageTextAccessibilityDescription[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
