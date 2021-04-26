// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Shows settings for adjusting scroll acceleration/sensitivity for
// mouse/touchpad.
const base::Feature kAllowScrollSettings{"AllowScrollSettings",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Ambient mode feature.
const base::Feature kAmbientModeFeature{"ChromeOSAmbientMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable ARC ADB sideloading support.
const base::Feature kArcAdbSideloadingFeature{
    "ArcAdbSideloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for ARC ADB sideloading for managed
// accounts and/or devices.
const base::Feature kArcManagedAdbSideloadingSupport{
    "ArcManagedAdbSideloadingSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for View.onKeyPreIme() of ARC apps.
const base::Feature kArcPreImeKeyEventSupport{
    "ArcPreImeKeyEventSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable assistive autocorrect.
const base::Feature kAssistAutoCorrect{"AssistAutoCorrect",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assist personal information.
const base::Feature kAssistPersonalInfo{"AssistPersonalInfo",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Displays the avatar toolbar button and the profile menu.
// https://crbug.com/1041472
extern const base::Feature kAvatarToolbarButton{
    "AvatarToolbarButton", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables more aggressive filtering out of Bluetooth devices with
// "appearances" that are less likely to be pairable or useful.
const base::Feature kBluetoothAggressiveAppearanceFilter{
    "BluetoothAggressiveAppearanceFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the usage of fixed Bluetooth A2DP packet size to improve
// audio performance in noisy environment.
const base::Feature kBluetoothFixA2dpPacketSize{
    "BluetoothFixA2dpPacketSize", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, browser will notify Chrome OS audio server to register HFP 1.7
// to BlueZ, which includes wideband speech feature.
const base::Feature kBluetoothNextHandsfreeProfile{
    "BluetoothNextHandsfreeProfile", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disables running the Camera App as a System Web App.
const base::Feature kCameraSystemWebApp{"CameraSystemWebApp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, options page for each input method will be opened in ChromeOS
// settings. Otherwise it will be opened in a new web page in Chrome browser.
const base::Feature kImeOptionsInSettings{"ImeOptionsInSettings",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini port forwarding.
const base::Feature kCrostiniPortForwarding{"CrostiniPortForwarding",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini Disk Resizing.
const base::Feature kCrostiniDiskResizing{"CrostiniDiskResizing",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini using Buster container images.
const base::Feature kCrostiniUseBusterImage{"CrostiniUseBusterImage",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the option to share the mic with Crostini or not
const base::Feature kCrostiniShowMicSetting{"CrostiniShowMicSetting",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini usb mounting for unsupported devices.
const base::Feature kCrostiniUsbAllowUnsupported{
    "CrostiniUsbAllowUnsupported", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the new WebUI Crostini upgrader.
const base::Feature kCrostiniWebUIUpgrader{"CrostiniWebUIUpgrader",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
const base::Feature kCryptAuthV2DeviceActivityStatus{
    "CryptAuthV2DeviceActivityStatus", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 DeviceSync flow. Regardless of this
// flag, v1 DeviceSync will continue to operate until it is disabled via the
// feature flag kDisableCryptAuthV1DeviceSync.
const base::Feature kCryptAuthV2DeviceSync{"CryptAuthV2DeviceSync",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Disables the CryptAuth v1 DeviceSync flow. Note: During the first phase
// of the v2 DeviceSync rollout, v1 and v2 DeviceSync run in parallel. This flag
// is needed to disable the v1 service during the second phase of the rollout.
// kCryptAuthV2DeviceSync should be enabled before this flag is flipped.
const base::Feature kDisableCryptAuthV1DeviceSync{
    "DisableCryptAuthV1DeviceSync", base::FEATURE_DISABLED_BY_DEFAULT};

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const base::Feature kDisableOfficeEditingComponentApp{
    "DisableOfficeEditingComponentApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Discover Application on Chrome OS.
// If enabled, Discover App will be shown in launcher.
const base::Feature kDiscoverApp{"DiscoverApp",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables DriveFS' experimental local files mirroring functionality.
const base::Feature kDriveFsMirroring{"DriveFsMirroring",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the DLC Settings subpage in Device section of OS Settings.
const base::Feature kDlcSettingsUi{"DlcSettingsUi",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, allows Unicorn users to add secondary EDU accounts.
const base::Feature kEduCoexistence{"EduCoexistence",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables parent consent logging in EDU account addition flow.
const base::Feature kEduCoexistenceConsentLog{"EduCoexistenceConsentLog",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, emoji suggestion will be shown when user type "space".
const base::Feature kEmojiSuggestAddition{"EmojiSuggestAddition",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Device End Of Lifetime warning notifications.
const base::Feature kEolWarningNotifications{"EolWarningNotifications",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable pointer lock for Crostini windows.
const base::Feature kExoPointerLock{"ExoPointerLock",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the next generation file manager.
const base::Feature kFilesNG{"FilesNG", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable file transfer details in progress center.
const base::Feature kFilesTransferDetails{"FilesTransferDetails",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new ZIP archive handling in Files App.
// https://crbug.com/912236
const base::Feature kFilesZipNoNaCl{"FilesZipNoNaCl",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to launch IME service with an 'ime' sandbox.
const base::Feature kEnableImeSandbox{"EnableImeSandbox",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, will display blocking screens during re-authentication after a
// supervision transition occurred.
const base::Feature kEnableSupervisionTransitionScreens{
    "EnableSupervisionTransitionScreens", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable restriction of symlink traversal on user-supplied filesystems.
const base::Feature kFsNosymfollow{"FsNosymfollow",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a D-Bus service for accessing gesture properties.
const base::Feature kGesturePropertiesDBusService{
    "GesturePropertiesDBusService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable primary/secondary action buttons on Gaia login screen.
const base::Feature kGaiaActionButtons{"GaiaActionButtons",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// The new ChromeOS Help App. https://crbug.com/1012578.
const base::Feature kHelpAppV2{"HelpAppV2", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the help app in the first run experience. This opens the help app
// after the OOBE, and provides some extra functionality like a getting started
// guide inside the app.
const base::Feature kHelpAppFirstRun{"HelpAppFirstRun",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for HMM decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicHmm{"ImeInputLogicHmm",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for FST decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicFst{"ImeInputLogicFst",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for Mozc decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicMozc{"ImeInputLogicMozc",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable using the floating virtual keyboard as the default option
// on Chrome OS.
const base::Feature kVirtualKeyboardFloatingDefault{
    "VirtualKeyboardFloatingDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables "Linux and Chrome OS" support. Allows a Linux version of Chrome
// ("lacros-chrome") to run as a Wayland client with this instance of Chrome
// ("ash-chrome") acting as the Wayland server and window manager.
const base::Feature kLacrosSupport{"LacrosSupport",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables device management disclosure on login / lock screen.
const base::Feature kLoginDeviceManagementDisclosure{
    "LoginDeviceManagementDisclosure", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the display password button on login / lock screen.
const base::Feature kLoginDisplayPasswordButton{
    "LoginDisplayPasswordButton", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the requirement of a minimum chrome version on the
// device through the policy MinimumChromeVersionEnforced. If the requirement is
// not met and the warning time in the policy has expired, the user is
// restricted from using the session.
const base::Feature kMinimumChromeVersion{"MinimumChromeVersion",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// ChromeOS Media App. https://crbug.com/996088.
const base::Feature kMediaApp{"MediaApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable native typing for rule-based input methods.
const base::Feature kNativeRuleBasedTyping{"NativeRuleBasedTyping",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to use screen priorities to decide if transition from one
// Oobe screen to another is allowed.
// TODO(https://crbug.com/1064271): Remove this flag once the feature is stable.
const base::Feature kOobeScreensPriority{"OobeScreensPriority",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable OS Settings fuzzy search, and disable search using
// exact string matching.
const base::Feature kNewOsSettingsSearch{"NewOsSettingsSearch",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable the Parental Controls section of settings.
const base::Feature kParentalControlsSettings{
    "ChromeOSParentalControlsSettings", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the camera permissions should be shown in the Plugin
// VM app settings.
const base::Feature kPluginVmShowCameraPermissions{
    "PluginVmShowCameraPermissions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the microphone permissions should be shown in the Plugin
// VM app settings.
const base::Feature kPluginVmShowMicrophonePermissions{
    "PluginVmShowMicrophonePermissions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to show printer statuses.
const base::Feature kPrinterStatus{"PrinterStatus",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the Print Job Management App.
const base::Feature kPrintJobManagementApp{"PrintJobManagementApp",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable quick answers.
const base::Feature kQuickAnswers{"QuickAnswers",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers rich ui.
const base::Feature kQuickAnswersRichUi{"QuickAnswersRichUi",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether dogfood version of quick answers.
const base::Feature kQuickAnswersDogfood{"QuickAnswersDogfood",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers text annotator.
const base::Feature kQuickAnswersTextAnnotator{
    "QuickAnswersTextAnnotator", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers setting sub toggle.
const base::Feature kQuickAnswersSubToggle{"QuickAnswersSubToggle",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers translation.
const base::Feature kQuickAnswersTranslation{"QuickAnswersTranslation",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// ChromeOS Files App mounts RAR archives via rar2fs instead of avfs.
// https://crbug.com/996549
const base::Feature kRar2Fs{"Rar2Fs", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes on Chrome OS.
const base::Feature kReleaseNotes{"ReleaseNotes",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on Chrome OS.
const base::Feature kReleaseNotesNotification{"ReleaseNotesNotification",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables an experimental scanning UI on Chrome OS.
const base::Feature kScanningUI{"ScanningUI",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
const base::Feature kSessionManagerLongKillTimeout{
    "SessionManagerLongKillTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the shelf hotseat.
const base::Feature kShelfHotseat{"ShelfHotseat",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables a toggle to enable Bluetooth debug logs.
const base::Feature kShowBluetoothDebugLogToggle{
    "ShowBluetoothDebugLogToggle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables showing the battery level in the System Tray and Settings
// UI for supported Bluetooth Devices.
const base::Feature kShowBluetoothDeviceBattery{
    "ShowBluetoothDeviceBattery", base::FEATURE_DISABLED_BY_DEFAULT};

// Shows the Play Store icon in Demo Mode.
const base::Feature kShowPlayInDemoMode{"ShowPlayInDemoMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Shows individual steps during Demo Mode setup.
const base::Feature kShowStepsInDemoModeSetup{"ShowStepsInDemoModeSetup",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Uses experimental component version for smart dim.
const base::Feature kSmartDimExperimentalComponent{
    "SmartDimExperimentalComponent", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the smart dim component updater to provide smart dim model and
// preprocessor configuration.
const base::Feature kSmartDimNewMlAgent{"SmartDimNewMlAgent",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the V3 (~2019-05 era) Smart Dim model instead of the default V2
// (~2018-11) model.
const base::Feature kSmartDimModelV3{"SmartDimModelV3",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// This feature:
// - Creates a new "Sync your settings" section in Chrome OS settings
// - Moves app, wallpaper and Wi-Fi sync to OS settings
// - Provides a separate toggle for OS preferences, distinct from browser
//   preferences
// - Makes the OS ModelTypes run in sync transport mode, controlled by a
//   master pref for the OS sync feature
// - Updates the OOBE sync consent screen
//
// NOTE: The feature is rolling out via a client-side Finch trial, so the actual
// state will vary. See config in
// chrome/browser/chromeos/sync/split_settings_sync_field_trial.cc
const base::Feature kSplitSettingsSync{"SplitSettingsSync",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a settings UI toggle that controls Suggested Content status. Also
// enables a corresponding notice in the Launcher about Suggested Content.
const base::Feature kSuggestedContentToggle{"SuggestedContentToggle",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Chrome OS Telemetry Extension.
const base::Feature kTelemetryExtension{"TelemetryExtension",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables unified media view in Files app to browse recently-modified media
// files from local local, Google Drive, and Android.
const base::Feature kUnifiedMediaView{"UnifiedMediaView",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the updated cellular activation UI; see go/cros-cellular-design.
const base::Feature kUpdatedCellularActivationUi{
    "UpdatedCellularActivationUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the same browser sync consent dialog as Windows/Mac/Linux. Allows the
// user to fully opt-out of browser sync, including marking the IdentityManager
// primary account as unconsented. Requires SplitSettingsSync.
// NOTE: Call UseBrowserSyncConsent() to test the flag, see implementation.
const base::Feature kUseBrowserSyncConsent{"UseBrowserSyncConsent",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Remap search+click to right click instead of the legacy alt+click on
// Chrome OS.
const base::Feature kUseSearchClickForRightClick{
    "UseSearchClickForRightClick", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable bordered key for virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardBorderedKey{
    "VirtualKeyboardBorderedKey", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable resizable floating virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardFloatingResizable{
    "VirtualKeyboardFloatingResizable", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable MOZC IME to use protobuf as interactive message format.
const base::Feature kImeMozcProto{"ImeMozcProto",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

////////////////////////////////////////////////////////////////////////////////

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsEduCoexistenceEnabled() {
  return base::FeatureList::IsEnabled(kEduCoexistence);
}

bool IsImeSandboxEnabled() {
  return base::FeatureList::IsEnabled(kEnableImeSandbox);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsLacrosSupportEnabled() {
  return base::FeatureList::IsEnabled(kLacrosSupport);
}

bool IsLoginDeviceManagementDisclosureEnabled() {
  return base::FeatureList::IsEnabled(kLoginDeviceManagementDisclosure);
}

bool IsLoginDisplayPasswordButtonEnabled() {
  return base::FeatureList::IsEnabled(kLoginDisplayPasswordButton);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
}

bool IsOobeScreensPriorityEnabled() {
  return base::FeatureList::IsEnabled(kOobeScreensPriority);
}

bool IsParentalControlsSettingsEnabled() {
  return base::FeatureList::IsEnabled(kParentalControlsSettings);
}

bool IsQuickAnswersDogfood() {
  return base::FeatureList::IsEnabled(kQuickAnswersDogfood);
}

bool IsQuickAnswersEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswers);
}

bool IsQuickAnswersRichUiEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersRichUi);
}

bool IsQuickAnswersSettingToggleEnabled() {
  return IsQuickAnswersEnabled() && IsQuickAnswersRichUiEnabled() &&
         base::FeatureList::IsEnabled(kQuickAnswersSubToggle);
}

bool IsQuickAnswersTextAnnotatorEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTextAnnotator);
}

bool IsQuickAnswersTranslationEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTranslation);
}

bool IsSplitSettingsSyncEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettingsSync);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool ShouldUseBrowserSyncConsent() {
  // UseBrowserSyncConsent requires SplitSettingsSync.
  return base::FeatureList::IsEnabled(kSplitSettingsSync) &&
         base::FeatureList::IsEnabled(kUseBrowserSyncConsent);
}

bool ShouldUseV1DeviceSync() {
  return !ShouldUseV2DeviceSync() ||
         !base::FeatureList::IsEnabled(
             chromeos::features::kDisableCryptAuthV1DeviceSync);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV2DeviceSync);
}

}  // namespace features
}  // namespace chromeos
