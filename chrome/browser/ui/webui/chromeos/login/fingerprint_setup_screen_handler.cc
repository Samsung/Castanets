// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId FingerprintSetupScreenView::kScreenId;

FingerprintSetupScreenHandler::FingerprintSetupScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.FingerprintSetupScreen.userActed");
}

FingerprintSetupScreenHandler::~FingerprintSetupScreenHandler() = default;

void FingerprintSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("setupFingerprintScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_TITLE);
  builder->Add("skipFingerprintSetup",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_SKIP);
  builder->Add("fingerprintSetupLater",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_LATER);
  builder->Add("fingerprintSetupNext",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_NEXT);
  builder->Add("fingerprintSetupDone",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_DONE);
  builder->Add("fingerprintSetupAddAnother",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_ADD_ANOTHER);
  builder->Add("placeFingerScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LOCATION_TITLE);
  builder->Add("enrollmentProgressScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_PROGRESS_TITLE);
  builder->Add("setupFingerprintEnrollmentSuccessTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_TITLE);
  builder->Add("setupFingerprintEnrollmentSuccessDescription",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_DESCRIPTION);
  builder->Add("setupFingerprintScanMoveFinger",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_MOVE_FINGER);
  builder->Add("setupFingerprintScanTryAgain",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_TRY_AGAIN);
  int description_id;
  switch (quick_unlock::GetFingerprintLocation()) {
    case quick_unlock::FingerprintLocation::TABLET_POWER_BUTTON:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_LEFT:
    case quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
    case quick_unlock::FingerprintLocation::KEYBOARD_TOP_RIGHT:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      break;
  }
  builder->Add("setupFingerprintScreenDescription", description_id);
}

void FingerprintSetupScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
}

void FingerprintSetupScreenHandler::Bind(FingerprintSetupScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void FingerprintSetupScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void FingerprintSetupScreenHandler::Hide() {}

void FingerprintSetupScreenHandler::Initialize() {}

void FingerprintSetupScreenHandler::OnEnrollScanDone(
    device::mojom::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  CallJS("login.FingerprintSetupScreen.onEnrollScanDone",
         static_cast<int>(scan_result), enroll_session_complete,
         percent_complete);
}

void FingerprintSetupScreenHandler::EnableAddAnotherFinger(bool enable) {
  CallJS("login.FingerprintSetupScreen.enableAddAnotherFinger", enable);
}

}  // namespace chromeos
