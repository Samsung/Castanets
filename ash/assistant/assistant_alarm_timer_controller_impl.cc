// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller_impl.h"

#include <map>
#include <string>
#include <utility>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/measunit.h"
#include "third_party/icu/source/i18n/unicode/measure.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using assistant::util::AlarmTimerAction;
using chromeos::assistant::features::IsTimersV2Enabled;
using chromeos::assistant::mojom::AssistantNotification;
using chromeos::assistant::mojom::AssistantNotificationButton;
using chromeos::assistant::mojom::AssistantNotificationButtonPtr;
using chromeos::assistant::mojom::AssistantNotificationPriority;
using chromeos::assistant::mojom::AssistantNotificationPtr;

// Grouping key and ID prefix for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";
constexpr char kTimerNotificationIdPrefix[] = "assistant/timer";

// Interval at which alarms/timers are ticked.
constexpr base::TimeDelta kTickInterval = base::TimeDelta::FromSeconds(1);

// Helpers ---------------------------------------------------------------------

std::string ToFormattedTimeString(base::TimeDelta time,
                                  UMeasureFormatWidth width) {
  DCHECK(width == UMEASFMT_WIDTH_NARROW || width == UMEASFMT_WIDTH_NUMERIC);

  // Method aliases to prevent line-wrapping below.
  const auto createHour = icu::MeasureUnit::createHour;
  const auto createMinute = icu::MeasureUnit::createMinute;
  const auto createSecond = icu::MeasureUnit::createSecond;

  // Calculate time in hours/minutes/seconds.
  const int64_t total_seconds = std::abs(time.InSeconds());
  const int32_t hours = total_seconds / 3600;
  const int32_t minutes = (total_seconds - hours * 3600) / 60;
  const int32_t seconds = total_seconds % 60;

  // Success of the ICU APIs is tracked by |status|.
  UErrorCode status = U_ZERO_ERROR;

  // Create our distinct |measures| to be formatted.
  std::vector<icu::Measure> measures;

  // We only show |hours| if necessary.
  if (hours)
    measures.push_back(icu::Measure(hours, createHour(status), status));

  // We only show |minutes| if necessary or if using numeric format |width|.
  if (minutes || width == UMEASFMT_WIDTH_NUMERIC)
    measures.push_back(icu::Measure(minutes, createMinute(status), status));

  // We only show |seconds| if necessary or if using numeric format |width|.
  if (seconds || width == UMEASFMT_WIDTH_NUMERIC)
    measures.push_back(icu::Measure(seconds, createSecond(status), status));

  // Format our |measures| into a |unicode_message|.
  icu::UnicodeString unicode_message;
  icu::FieldPosition field_position = icu::FieldPosition::DONT_CARE;
  icu::MeasureFormat measure_format(icu::Locale::getDefault(), width, status);
  measure_format.formatMeasures(measures.data(), measures.size(),
                                unicode_message, field_position, status);

  std::string formatted_time;
  if (U_SUCCESS(status)) {
    // If formatting was successful, convert our |unicode_message| into UTF-8.
    unicode_message.toUTF8String(formatted_time);
  } else {
    // If something went wrong formatting w/ ICU, fall back to I18N messages.
    LOG(ERROR) << "Error formatting time string: " << status;
    formatted_time =
        base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                width == UMEASFMT_WIDTH_NARROW
                    ? IDS_ASSISTANT_TIMER_NOTIFICATION_FORMATTED_TIME_NARROW_FALLBACK
                    : IDS_ASSISTANT_TIMER_NOTIFICATION_FORMATTED_TIME_NUMERIC_FALLBACK),
            hours, minutes, seconds));
  }

  // If necessary, negate the amount of time remaining.
  if (time.InSeconds() < 0) {
    formatted_time =
        base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_ASSISTANT_TIMER_NOTIFICATION_FORMATTED_TIME_NEGATE),
            formatted_time));
  }

  return formatted_time;
}

// Returns a string representation of the original duration for a given |timer|.
std::string ToOriginalDurationString(const AssistantTimer& timer) {
  return ToFormattedTimeString(timer.original_duration, UMEASFMT_WIDTH_NARROW);
}

// Returns a string representation of the remaining time for the given |timer|.
std::string ToRemainingTimeString(const AssistantTimer& timer) {
  return ToFormattedTimeString(timer.remaining_time, UMEASFMT_WIDTH_NUMERIC);
}

// Creates a notification ID for the given |timer|. It is guaranteed that this
// method will always return the same notification ID given the same timer.
std::string CreateTimerNotificationId(const AssistantTimer& timer) {
  return std::string(kTimerNotificationIdPrefix) + timer.id;
}

// Creates a notification title for the given |timer|.
std::string CreateTimerNotificationTitle(const AssistantTimer& timer) {
  if (IsTimersV2Enabled())
    return ToRemainingTimeString(timer);
  return l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
}

// Creates a notification message for the given |timer|.
std::string CreateTimerNotificationMessage(const AssistantTimer& timer) {
  if (IsTimersV2Enabled()) {
    if (timer.label.empty()) {
      return base::UTF16ToUTF8(
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(
                  timer.state == AssistantTimerState::kFired
                      ? IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_WHEN_FIRED
                      : IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE),
              ToOriginalDurationString(timer)));
    }
    return base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(
            timer.state == AssistantTimerState::kFired
                ? IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_WHEN_FIRED_WITH_LABEL
                : IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_WITH_LABEL),
        ToOriginalDurationString(timer), timer.label));
  }
  return ToRemainingTimeString(timer);
}

// Creates notification action URL for the given |timer|.
GURL CreateTimerNotificationActionUrl(const AssistantTimer& timer) {
  // In timers v2, clicking the notification does nothing.
  if (IsTimersV2Enabled())
    return GURL();
  // In timers v1, clicking the notification removes the |timer|.
  return assistant::util::CreateAlarmTimerDeepLink(
             AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
      .value();
}

// Creates notification buttons for the given |timer|.
std::vector<AssistantNotificationButtonPtr> CreateTimerNotificationButtons(
    const AssistantTimer& timer) {
  std::vector<AssistantNotificationButtonPtr> buttons;

  if (!IsTimersV2Enabled()) {
    // "STOP" button.
    buttons.push_back(AssistantNotificationButton::New(
        l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON),
        assistant::util::CreateAlarmTimerDeepLink(
            AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
            .value(),
        /*remove_notification_on_click=*/true));

    // "ADD 1 MIN" button.
    buttons.push_back(AssistantNotificationButton::New(
        l10n_util::GetStringUTF8(
            IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
        assistant::util::CreateAlarmTimerDeepLink(
            AlarmTimerAction::kAddTimeToTimer, timer.id,
            base::TimeDelta::FromMinutes(1))
            .value(),
        /*remove_notification_on_click=*/true));

    return buttons;
  }

  DCHECK(IsTimersV2Enabled());

  if (timer.state != AssistantTimerState::kFired) {
    if (timer.state == AssistantTimerState::kPaused) {
      // "RESUME" button.
      buttons.push_back(AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_RESUME_BUTTON),
          assistant::util::CreateAlarmTimerDeepLink(
              AlarmTimerAction::kResumeTimer, timer.id)
              .value(),
          /*remove_notification_on_click=*/false));
    } else {
      // "PAUSE" button.
      buttons.push_back(AssistantNotificationButton::New(
          l10n_util::GetStringUTF8(
              IDS_ASSISTANT_TIMER_NOTIFICATION_PAUSE_BUTTON),
          assistant::util::CreateAlarmTimerDeepLink(
              AlarmTimerAction::kPauseTimer, timer.id)
              .value(),
          /*remove_notification_on_click=*/false));
    }
  }

  // "CANCEL" button.
  buttons.push_back(AssistantNotificationButton::New(
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON),
      assistant::util::CreateAlarmTimerDeepLink(
          AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
          .value(),
      /*remove_notification_on_click=*/true));

  if (timer.state == AssistantTimerState::kFired) {
    // "ADD 1 MIN" button.
    buttons.push_back(AssistantNotificationButton::New(
        l10n_util::GetStringUTF8(
            IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
        assistant::util::CreateAlarmTimerDeepLink(
            AlarmTimerAction::kAddTimeToTimer, timer.id,
            base::TimeDelta::FromMinutes(1))
            .value(),
        /*remove_notification_on_click=*/false));
  }

  return buttons;
}

// Creates a timer notification priority for the given |timer|.
AssistantNotificationPriority CreateTimerNotificationPriority(
    const AssistantTimer& timer) {
  // In timers v1, all notifications are |kHigh| priority.
  if (!IsTimersV2Enabled())
    return AssistantNotificationPriority::kHigh;

  // In timers v2, a notification for a |kFired| timer is |kHigh| priority.
  // This will cause the notification to pop up to the user.
  if (timer.state == AssistantTimerState::kFired)
    return AssistantNotificationPriority::kHigh;

  // If the notification has lived for at least |kPopupThreshold|, drop the
  // priority to |kLow| so that the notification will not pop up to the user.
  constexpr base::TimeDelta kPopupThreshold = base::TimeDelta::FromSeconds(6);
  const base::TimeDelta lifetime =
      base::Time::Now() - timer.creation_time.value_or(base::Time::Now());
  if (lifetime >= kPopupThreshold)
    return AssistantNotificationPriority::kLow;

  // Otherwise, the notification is |kDefault| priority. This means that it
  // may or may not pop up to the user, depending on the presence of other
  // notifications.
  return AssistantNotificationPriority::kDefault;
}

// Creates a notification for the given |timer|.
AssistantNotificationPtr CreateTimerNotification(const AssistantTimer& timer) {
  AssistantNotificationPtr notification = AssistantNotification::New();
  notification->title = CreateTimerNotificationTitle(timer);
  notification->message = CreateTimerNotificationMessage(timer);
  notification->action_url = CreateTimerNotificationActionUrl(timer);
  notification->buttons = CreateTimerNotificationButtons(timer);
  notification->client_id = CreateTimerNotificationId(timer);
  notification->grouping_key = kTimerNotificationGroupingKey;
  notification->priority = CreateTimerNotificationPriority(timer);
  notification->remove_on_click = !IsTimersV2Enabled();
  notification->is_pinned = IsTimersV2Enabled();
  return notification;
}

}  // namespace

// AssistantAlarmTimerControllerImpl ------------------------------------------

AssistantAlarmTimerControllerImpl::AssistantAlarmTimerControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  model_.AddObserver(this);
  assistant_controller_observer_.Add(AssistantController::Get());
}

AssistantAlarmTimerControllerImpl::~AssistantAlarmTimerControllerImpl() {
  model_.RemoveObserver(this);
}

void AssistantAlarmTimerControllerImpl::SetAssistant(
    chromeos::assistant::Assistant* assistant) {
  assistant_ = assistant;
}

const AssistantAlarmTimerModel* AssistantAlarmTimerControllerImpl::GetModel()
    const {
  return &model_;
}

void AssistantAlarmTimerControllerImpl::OnTimerStateChanged(
    std::vector<AssistantTimerPtr> new_or_updated_timers) {
  // First we remove all old timers that no longer exist.
  for (const auto* old_timer : model_.GetAllTimers()) {
    if (std::none_of(new_or_updated_timers.begin(), new_or_updated_timers.end(),
                     [&old_timer](const auto& new_or_updated_timer) {
                       return old_timer->id == new_or_updated_timer->id;
                     })) {
      model_.RemoveTimer(old_timer->id);
    }
  }

  // Then we add any new timers and update existing ones.
  for (auto& new_or_updated_timer : new_or_updated_timers)
    model_.AddOrUpdateTimer(std::move(new_or_updated_timer));
}

void AssistantAlarmTimerControllerImpl::OnAssistantControllerConstructed() {
  AssistantState::Get()->AddObserver(this);
}

void AssistantAlarmTimerControllerImpl::OnAssistantControllerDestroying() {
  AssistantState::Get()->RemoveObserver(this);
}

void AssistantAlarmTimerControllerImpl::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type != DeepLinkType::kAlarmTimer)
    return;

  const base::Optional<AlarmTimerAction>& action =
      assistant::util::GetDeepLinkParamAsAlarmTimerAction(params);
  if (!action.has_value())
    return;

  const base::Optional<std::string>& alarm_timer_id =
      assistant::util::GetDeepLinkParam(params, DeepLinkParam::kId);
  if (!alarm_timer_id.has_value())
    return;

  // Duration is optional. Only used for adding time to timer.
  const base::Optional<base::TimeDelta>& duration =
      assistant::util::GetDeepLinkParamAsTimeDelta(params,
                                                   DeepLinkParam::kDurationMs);

  PerformAlarmTimerAction(action.value(), alarm_timer_id.value(), duration);
}

void AssistantAlarmTimerControllerImpl::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus status) {
  // If LibAssistant is no longer running we need to clear our cache to
  // accurately reflect LibAssistant alarm/timer state.
  if (status == chromeos::assistant::AssistantStatus::NOT_READY)
    model_.RemoveAllTimers();
}

void AssistantAlarmTimerControllerImpl::OnTimerAdded(
    const AssistantTimer& timer) {
  // Schedule a repeating timer to tick the tracked timers.
  if (!ticker_.IsRunning()) {
    ticker_.Start(FROM_HERE, kTickInterval, &model_,
                  &AssistantAlarmTimerModel::Tick);
  }

  // Create a notification for the added alarm/timer.
  assistant_controller_->notification_controller()->AddOrUpdateNotification(
      CreateTimerNotification(timer));
}

void AssistantAlarmTimerControllerImpl::OnTimerUpdated(
    const AssistantTimer& timer) {
  // When a |timer| is updated we need to update the corresponding notification
  // unless it has already been dismissed by the user.
  auto* notification_controller =
      assistant_controller_->notification_controller();
  if (notification_controller->model()->HasNotificationForId(
          CreateTimerNotificationId(timer))) {
    notification_controller->AddOrUpdateNotification(
        CreateTimerNotification(timer));
  }
}

void AssistantAlarmTimerControllerImpl::OnTimerRemoved(
    const AssistantTimer& timer) {
  // If our model is empty, we no longer need tick updates.
  if (model_.empty())
    ticker_.Stop();

  // Remove any notification associated w/ |timer|.
  assistant_controller_->notification_controller()->RemoveNotificationById(
      CreateTimerNotificationId(timer), /*from_server=*/false);
}

void AssistantAlarmTimerControllerImpl::PerformAlarmTimerAction(
    const AlarmTimerAction& action,
    const std::string& alarm_timer_id,
    const base::Optional<base::TimeDelta>& duration) {
  DCHECK(assistant_);

  switch (action) {
    case AlarmTimerAction::kAddTimeToTimer:
      if (!duration.has_value()) {
        LOG(ERROR) << "Ignoring add time to timer action duration.";
        return;
      }
      assistant_->AddTimeToTimer(alarm_timer_id, duration.value());
      break;
    case AlarmTimerAction::kPauseTimer:
      DCHECK(!duration.has_value());
      assistant_->PauseTimer(alarm_timer_id);
      break;
    case AlarmTimerAction::kRemoveAlarmOrTimer:
      DCHECK(!duration.has_value());
      assistant_->RemoveAlarmOrTimer(alarm_timer_id);
      break;
    case AlarmTimerAction::kResumeTimer:
      DCHECK(!duration.has_value());
      assistant_->ResumeTimer(alarm_timer_id);
      break;
  }
}

}  // namespace ash
