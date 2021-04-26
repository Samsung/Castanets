// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include <cmath>

#include "ash/public/cpp/system_tray.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler_delegate_impl.h"
#include "chrome/browser/chromeos/ui/update_required_notification.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

using MinimumVersionRequirement =
    policy::MinimumVersionPolicyHandler::MinimumVersionRequirement;

namespace policy {

namespace {

const int kOneWeekEolNotificationInDays = 7;

PrefService* local_state() {
  return g_browser_process->local_state();
}

MinimumVersionPolicyHandler::NetworkStatus GetCurrentNetworkStatus() {
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* current_network =
      network_state_handler->DefaultNetwork();
  if (!current_network || !current_network->IsConnectedState())
    return MinimumVersionPolicyHandler::NetworkStatus::kOffline;
  if (network_state_handler->default_network_is_metered())
    return MinimumVersionPolicyHandler::NetworkStatus::kMetered;
  return MinimumVersionPolicyHandler::NetworkStatus::kAllowed;
}

void OpenNetworkSettings() {
  ash::SystemTray::Get()->ShowNetworkDetailedViewBubble(
      true /* show_by_click */);
}

void OpenEnterpriseInfoPage() {
  SystemTrayClient::Get()->ShowEnterpriseInfo();
}

std::string GetEnterpriseDomainName() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetEnterpriseDisplayDomain();
}

BuildState* GetBuildState() {
  return g_browser_process->GetBuildState();
}

int GetDaysRounded(base::TimeDelta time) {
  return std::lround(time.InSecondsF() /
                     base::TimeDelta::FromDays(1).InSecondsF());
}

chromeos::UpdateEngineClient* GetUpdateEngineClient() {
  return chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
}

}  // namespace

const char MinimumVersionPolicyHandler::kChromeVersion[] = "chrome_version";
const char MinimumVersionPolicyHandler::kWarningPeriod[] = "warning_period";
const char MinimumVersionPolicyHandler::KEolWarningPeriod[] =
    "eol_warning_period";

MinimumVersionRequirement::MinimumVersionRequirement(
    const base::Version version,
    const base::TimeDelta warning,
    const base::TimeDelta eol_warning)
    : minimum_version_(version),
      warning_time_(warning),
      eol_warning_time_(eol_warning) {}

std::unique_ptr<MinimumVersionRequirement>
MinimumVersionRequirement::CreateInstanceIfValid(
    const base::DictionaryValue* dict) {
  const std::string* version = dict->FindStringPath(kChromeVersion);
  if (!version)
    return nullptr;
  base::Version minimum_version(*version);
  if (!minimum_version.IsValid())
    return nullptr;
  auto warning = dict->FindIntPath(kWarningPeriod);
  base::TimeDelta warning_time =
      base::TimeDelta::FromDays(warning.has_value() ? warning.value() : 0);
  auto eol_warning = dict->FindIntPath(KEolWarningPeriod);
  base::TimeDelta eol_warning_time = base::TimeDelta::FromDays(
      eol_warning.has_value() ? eol_warning.value() : 0);
  return std::make_unique<MinimumVersionRequirement>(
      minimum_version, warning_time, eol_warning_time);
}

int MinimumVersionRequirement::Compare(
    const MinimumVersionRequirement* other) const {
  const int version_compare = version().CompareTo(other->version());
  if (version_compare != 0)
    return version_compare;
  if (warning() != other->warning())
    return (warning() > other->warning() ? 1 : -1);
  if (eol_warning() != other->eol_warning())
    return (eol_warning() > other->eol_warning() ? 1 : -1);
  return 0;
}

MinimumVersionPolicyHandler::MinimumVersionPolicyHandler(
    Delegate* delegate,
    chromeos::CrosSettings* cros_settings)
    : delegate_(delegate),
      cros_settings_(cros_settings),
      clock_(base::DefaultClock::GetInstance()) {
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kMinimumChromeVersionEnforced,
      base::Bind(&MinimumVersionPolicyHandler::OnPolicyChanged,
                 weak_factory_.GetWeakPtr()));

  // Fire it once so we're sure we get an invocation on startup.
  OnPolicyChanged();
}

MinimumVersionPolicyHandler::~MinimumVersionPolicyHandler() {
  GetBuildState()->RemoveObserver(this);
  StopObservingNetwork();
  GetUpdateEngineClient()->RemoveObserver(this);
}

void MinimumVersionPolicyHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MinimumVersionPolicyHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MinimumVersionPolicyHandler::CurrentVersionSatisfies(
    const MinimumVersionRequirement& requirement) const {
  return delegate_->GetCurrentVersion().CompareTo(requirement.version()) >= 0;
}

//  static
void MinimumVersionPolicyHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kUpdateRequiredTimerStartTime,
                             base::Time());
  registry->RegisterTimeDeltaPref(prefs::kUpdateRequiredWarningPeriod,
                                  base::TimeDelta());
}

bool MinimumVersionPolicyHandler::IsDeadlineTimerRunningForTesting() {
  return update_required_deadline_timer_.IsRunning();
}

bool MinimumVersionPolicyHandler::IsPolicyApplicable() {
  bool device_managed = delegate_->IsEnterpriseManaged();
  bool is_kiosk = delegate_->IsKioskMode();
  return device_managed && !is_kiosk;
}

void MinimumVersionPolicyHandler::OnPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&MinimumVersionPolicyHandler::OnPolicyChanged,
                         weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED ||
      !IsPolicyApplicable() ||
      !chromeos::features::IsMinimumChromeVersionEnabled())
    return;

  const base::ListValue* entries;
  std::vector<std::unique_ptr<MinimumVersionRequirement>> configs;
  if (!cros_settings_->GetList(chromeos::kMinimumChromeVersionEnforced,
                               &entries) ||
      !entries->GetSize()) {
    // Reset state and hide update required screen if policy is not set or set
    // to empty list.
    HandleUpdateNotRequired();
    return;
  }

  for (const auto& item : entries->GetList()) {
    const base::DictionaryValue* dict;
    if (item.GetAsDictionary(&dict)) {
      std::unique_ptr<MinimumVersionRequirement> instance =
          MinimumVersionRequirement::CreateInstanceIfValid(dict);
      if (instance)
        configs.push_back(std::move(instance));
    }
  }

  // Select the strongest config whose requirements are not satisfied by the
  // current version. The strongest config is chosen as the one whose minimum
  // required version is greater and closest to the current version. In case of
  // conflict. preference is given to the one with lesser warning time or eol
  // warning time.
  int strongest_config_idx = -1;
  std::vector<std::unique_ptr<MinimumVersionRequirement>>
      update_required_configs;
  for (unsigned int i = 0; i < configs.size(); i++) {
    MinimumVersionRequirement* item = configs[i].get();
    if (!CurrentVersionSatisfies(*item) &&
        (strongest_config_idx == -1 ||
         item->Compare(configs[strongest_config_idx].get()) < 0))
      strongest_config_idx = i;
  }

  if (strongest_config_idx != -1) {
    // Update is required if at least one config exists whose requirements are
    // not satisfied by the current version.
    std::unique_ptr<MinimumVersionRequirement> strongest_config =
        std::move(configs[strongest_config_idx]);
    if (!state_ || state_->Compare(strongest_config.get()) != 0) {
      state_ = std::move(strongest_config);
      FetchEolInfo();
    }
  } else {
    // Update is not required as the requirements of all of the configs in the
    // policy are satisfied by the current chrome version. We could also reach
    // here at the time of login if the device was rebooted to apply the
    // downloaded update, in which case it is needed to reset the local state.
    HandleUpdateNotRequired();
  }
}

void MinimumVersionPolicyHandler::HandleUpdateNotRequired() {
  // Reset the state including any running timers.
  Reset();
  // Hide update required screen if it is visible and switch back to the login
  // screen.
  if (delegate_->IsLoginSessionState()) {
    delegate_->HideUpdateRequiredScreenIfShown();
  }
}

void MinimumVersionPolicyHandler::Reset() {
  deadline_reached = false;
  eol_reached_ = false;
  update_required_deadline_ = base::Time();
  update_required_deadline_timer_.Stop();
  notification_timer_.Stop();
  GetBuildState()->RemoveObserver(this);
  state_.reset();
  HideNotification();
  notification_handler_.reset();
  ResetLocalState();
  StopObservingNetwork();
}

void MinimumVersionPolicyHandler::ResetOnUpdateCompleted() {
  update_required_deadline_timer_.Stop();
  notification_timer_.Stop();
  GetBuildState()->RemoveObserver(this);
  HideNotification();
  notification_handler_.reset();
}

void MinimumVersionPolicyHandler::FetchEolInfo() {
  // Return if update required state is null meaning all requirements are
  // satisfied.
  if (!state_)
    return;

  update_required_time_ = clock_->Now();
  // Request the End of Life (Auto Update Expiration) status.
  GetUpdateEngineClient()->GetEolInfo(
      base::BindOnce(&MinimumVersionPolicyHandler::OnFetchEolInfo,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::OnFetchEolInfo(
    const chromeos::UpdateEngineClient::EolInfo info) {
  if (info.eol_date.is_null() || info.eol_date > update_required_time_) {
    // End of life is not reached. Start update with |warning_time_|.
    eol_reached_ = false;
    HandleUpdateRequired(state_->warning());
  } else {
    // End of life is reached. Start update with |eol_warning_time_|.
    eol_reached_ = true;
    HandleUpdateRequired(state_->eol_warning());
  }
  if (!fetch_eol_callback_.is_null())
    std::move(fetch_eol_callback_).Run();
}

void MinimumVersionPolicyHandler::HandleUpdateRequired(
    base::TimeDelta warning_time) {
  const base::Time stored_timer_start_time =
      local_state()->GetTime(prefs::kUpdateRequiredTimerStartTime);
  const base::TimeDelta stored_warning_time =
      local_state()->GetTimeDelta(prefs::kUpdateRequiredWarningPeriod);
  base::Time previous_deadline = stored_timer_start_time + stored_warning_time;

  // If update is already required, use the existing timer start time to
  // calculate the new deadline. Else use |update_required_time_|. Do not reduce
  // the warning time if policy is already applied.
  if (stored_timer_start_time.is_null()) {
    update_required_deadline_ = update_required_time_ + warning_time;
  } else {
    update_required_deadline_ =
        stored_timer_start_time + std::max(stored_warning_time, warning_time);
  }

  const bool deadline_reached =
      update_required_deadline_ <= update_required_time_;
  if (deadline_reached) {
    // As per the policy, the deadline for the user cannot reduce.
    // This case can be encountered when :-
    // a) Update was not required before and now critical update is required.
    // b) Update was required and warning time has expired when device is
    // rebooted.
    OnDeadlineReached();
    return;
  }

  // Need to start the timer even if the deadline is same as the previous one to
  // handle the case of Chrome reboot.
  if (update_required_deadline_ == previous_deadline &&
      update_required_deadline_timer_.IsRunning()) {
    return;
  }

  // This case can be encountered when :-
  // a) Update was not required before and now update is required with a
  // warning time. b) Policy has been updated with new values and update is
  // still required.

  // Hide update required screen if it is shown on the login screen.
  if (delegate_->IsLoginSessionState()) {
    delegate_->HideUpdateRequiredScreenIfShown();
  }
  // The |deadline| can only be equal to or greater than the
  // |previous_deadline|. No need to update the local state if the deadline has
  // not been extended.
  if (update_required_deadline_ > previous_deadline)
    UpdateLocalState(warning_time);

  // The device has already downloaded the update in-session and waiting for
  // reboot to apply it.
  if (GetBuildState()->update_type() == BuildState::UpdateType::kNormalUpdate) {
    // TODO(https://crbug.com/1048607): May be adjust relaunch notification
    // timer as per new deadline.
    return;
  }

  StartDeadlineTimer(update_required_deadline_);
  if (!eol_reached_)
    StartObservingUpdate();
  ShowAndScheduleNotification(update_required_deadline_);
}

void MinimumVersionPolicyHandler::ResetLocalState() {
  local_state()->ClearPref(prefs::kUpdateRequiredTimerStartTime);
  local_state()->ClearPref(prefs::kUpdateRequiredWarningPeriod);
}

void MinimumVersionPolicyHandler::UpdateLocalState(
    base::TimeDelta warning_time) {
  base::Time timer_start_time =
      local_state()->GetTime(prefs::kUpdateRequiredTimerStartTime);
  if (timer_start_time.is_null()) {
    local_state()->SetTime(prefs::kUpdateRequiredTimerStartTime,
                           update_required_time_);
  }
  local_state()->SetTimeDelta(prefs::kUpdateRequiredWarningPeriod,
                              warning_time);
  local_state()->CommitPendingWrite();
}

void MinimumVersionPolicyHandler::StartDeadlineTimer(base::Time deadline) {
  // Start the timer to expire when deadline is reached and the device is not
  // updated to meet the policy requirements.
  update_required_deadline_timer_.Start(
      FROM_HERE, deadline,
      base::BindOnce(&MinimumVersionPolicyHandler::OnDeadlineReached,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::StartObservingUpdate() {
  auto* build_state = GetBuildState();
  if (!build_state->HasObserver(this))
    build_state->AddObserver(this);
}

void MinimumVersionPolicyHandler::MaybeShowNotificationOnLogin() {
  const base::Time now = clock_->Now();
  // This should only be true if |update_required_deadline_timer_| expired while
  // login was in progress, else we would have shown the update required screen
  // at startup.
  if (update_required_deadline_ <= now)
    return;

  base::TimeDelta time_remaining = update_required_deadline_ - now;
  int days_remaining = GetDaysRounded(time_remaining);
  if (days_remaining <= 1)
    MaybeShowNotification(base::TimeDelta::FromDays(days_remaining));
}

void MinimumVersionPolicyHandler::MaybeShowNotification(
    base::TimeDelta warning) {
  const NetworkStatus status = GetCurrentNetworkStatus();
  if ((!eol_reached_ && status == NetworkStatus::kAllowed) ||
      !delegate_->IsUserLoggedIn() || !delegate_->IsUserManaged()) {
    return;
  }

  if (!notification_handler_) {
    notification_handler_ =
        std::make_unique<chromeos::UpdateRequiredNotification>();
  }

  NotificationType type = NotificationType::kNoConnection;
  base::OnceClosure button_click_callback;
  std::string domain_name = GetEnterpriseDomainName();
  auto close_callback =
      base::BindOnce(&MinimumVersionPolicyHandler::StopObservingNetwork,
                     weak_factory_.GetWeakPtr());
  if (eol_reached_) {
    type = NotificationType::kEolReached;
    button_click_callback = base::BindOnce(&OpenEnterpriseInfoPage);
  } else if (status == NetworkStatus::kMetered) {
    type = NotificationType::kMeteredConnection;
    button_click_callback = base::BindOnce(
        &MinimumVersionPolicyHandler::UpdateOverMeteredPermssionGranted,
        weak_factory_.GetWeakPtr());
  } else if (status == NetworkStatus::kOffline) {
    button_click_callback = base::BindOnce(&OpenNetworkSettings);
  } else {
    NOTREACHED();
    return;
  }
  notification_handler_->Show(type, warning, domain_name,
                              std::move(button_click_callback),
                              std::move(close_callback));

  if (!eol_reached_) {
    chromeos::NetworkStateHandler* network_state_handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    if (!network_state_handler->HasObserver(this))
      network_state_handler->AddObserver(this, FROM_HERE);
  }
}

void MinimumVersionPolicyHandler::ShowAndScheduleNotification(
    base::Time deadline) {
  const base::Time now = clock_->Now();
  if (deadline <= now)
    return;

  base::Time expiry;
  base::TimeDelta time_remaining = deadline - now;
  int days_remaining = GetDaysRounded(time_remaining);

  // Network limitation notifications are shown when policy is received and on
  // the last day. End of life notifications are shown when policy is received,
  // one week before EOL and on the last day. No need to schedule a notification
  // if it is already the last day.
  if (eol_reached_ && days_remaining > kOneWeekEolNotificationInDays) {
    expiry =
        deadline - base::TimeDelta::FromDays(kOneWeekEolNotificationInDays);
  } else if (days_remaining > 1) {
    expiry = deadline - base::TimeDelta::FromDays(1);
  }

  MaybeShowNotification(base::TimeDelta::FromDays(days_remaining));
  if (!expiry.is_null()) {
    notification_timer_.Start(
        FROM_HERE, expiry,
        base::BindOnce(
            &MinimumVersionPolicyHandler::ShowAndScheduleNotification,
            weak_factory_.GetWeakPtr(), deadline));
  }
}

void MinimumVersionPolicyHandler::OnUpdate(const BuildState* build_state) {
  // If the device has been successfully updated, the relaunch notifications
  // will reboot it for applying the updates.
  GetUpdateEngineClient()->RemoveObserver(this);
  if (build_state->update_type() == BuildState::UpdateType::kNormalUpdate)
    ResetOnUpdateCompleted();
}

void MinimumVersionPolicyHandler::HideNotification() const {
  if (notification_handler_)
    notification_handler_->Hide();
}

void MinimumVersionPolicyHandler::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  // Close notification if network has switched to one that allows updates.
  const NetworkStatus status = GetCurrentNetworkStatus();
  if (status == NetworkStatus::kAllowed && notification_handler_) {
    HideNotification();
  }
}

void MinimumVersionPolicyHandler::StopObservingNetwork() {
  if (!chromeos::NetworkHandler::IsInitialized())
    return;
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  network_state_handler->RemoveObserver(this, FROM_HERE);
}

void MinimumVersionPolicyHandler::UpdateOverMeteredPermssionGranted() {
  chromeos::UpdateEngineClient* const update_engine_client =
      GetUpdateEngineClient();
  if (!update_engine_client->HasObserver(this))
    update_engine_client->AddObserver(this);
  update_engine_client->RequestUpdateCheck(
      base::BindOnce(&MinimumVersionPolicyHandler::OnUpdateCheckStarted,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::OnUpdateCheckStarted(
    chromeos::UpdateEngineClient::UpdateCheckResult result) {
  if (result != chromeos::UpdateEngineClient::UPDATE_RESULT_SUCCESS)
    GetUpdateEngineClient()->RemoveObserver(this);
}

void MinimumVersionPolicyHandler::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  if (status.current_operation() ==
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE) {
    GetUpdateEngineClient()->SetUpdateOverCellularOneTimePermission(
        status.new_version(), status.new_size(),
        base::BindOnce(&MinimumVersionPolicyHandler::
                           OnSetUpdateOverCellularOneTimePermission,
                       weak_factory_.GetWeakPtr()));
  }
}

void MinimumVersionPolicyHandler::OnSetUpdateOverCellularOneTimePermission(
    bool success) {
  if (success)
    UpdateOverMeteredPermssionGranted();
  else
    GetUpdateEngineClient()->RemoveObserver(this);
}

void MinimumVersionPolicyHandler::OnDeadlineReached() {
  deadline_reached = true;
  if (delegate_->IsLoginSessionState() && !delegate_->IsLoginInProgress()) {
    // Show update required screen over the login screen.
    delegate_->ShowUpdateRequiredScreen();
  } else if (delegate_->IsUserLoggedIn() && delegate_->IsUserManaged()) {
    // Terminate the current user session to show update required
    // screen on the login screen if user is managed.
    delegate_->RestartToLoginScreen();
  }
  // No action is required if -
  // 1) The user signed in is not managed. Once the un-managed user signs out or
  // the device is rebooted, the policy handler will be called again to show the
  // update required screen if required.
  // 2) Login is in progress. This would be handled in-session once user logs
  // in, the user would be logged out and update required screen is shown.
  // 3) Device has just been enrolled. The login screen would check and show the
  // update required screen.
}

}  // namespace policy
