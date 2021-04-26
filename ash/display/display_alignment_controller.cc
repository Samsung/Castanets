// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_alignment_controller.h"

#include "ash/display/display_alignment_indicator.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

// Number of times the mouse has to hit the edge to show the indicators.
constexpr int kTriggerThresholdCount = 2;
// Time between last time the mouse leaves a screen edge and the counter
// resetting.
constexpr base::TimeDelta kCounterResetTime = base::TimeDelta::FromSeconds(1);
// How long the indicators are visible for.
constexpr base::TimeDelta kIndicatorVisibilityDuration =
    base::TimeDelta::FromSeconds(2);

// Returns true if |screen_location| is on the edge of |display|. |display| must
// be valid.
bool IsOnBoundary(const gfx::Point& screen_location,
                  const display::Display& display) {
  DCHECK(display.is_valid());

  const gfx::Rect& bounds = display.bounds();

  const int top = bounds.y();
  const int bottom = bounds.bottom() - 1;
  const int left = bounds.x();
  const int right = bounds.right() - 1;

  // See if current screen_location is within 1px of the display's
  // borders. 1px leniency is necessary as some resolution/size factor
  // combination results in the mouse not being able to reach the edges of the
  // display by 1px.
  if (std::abs(screen_location.x() - left) <= 1)
    return true;
  if (std::abs(screen_location.x() - right) <= 1)
    return true;
  if (std::abs(screen_location.y() - top) <= 1)
    return true;

  return std::abs(screen_location.y() - bottom) <= 1;
}

}  // namespace

DisplayAlignmentController::DisplayAlignmentController()
    : action_trigger_timer_(std::make_unique<base::OneShotTimer>()) {
  Shell* shell = Shell::Get();
  shell->AddPreTargetHandler(this);
  shell->session_controller()->AddObserver(this);
  shell->window_tree_host_manager()->AddObserver(this);

  is_locked_ = shell->session_controller()->IsScreenLocked();

  RefreshState();
}

DisplayAlignmentController::~DisplayAlignmentController() {
  Shell* shell = Shell::Get();
  shell->window_tree_host_manager()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  shell->RemovePreTargetHandler(this);
}

void DisplayAlignmentController::OnDisplayConfigurationChanged() {
  RefreshState();
}

void DisplayAlignmentController::OnDisplaysInitialized() {
  RefreshState();
}

void DisplayAlignmentController::OnMouseEvent(ui::MouseEvent* event) {
  if (current_state_ == DisplayAlignmentState::kDisabled ||
      event->type() != ui::ET_MOUSE_MOVED) {
    return;
  }

  // If mouse enters the edge of the display.
  const gfx::Point screen_location = event->target()->GetScreenLocation(*event);

  const display::Display& src_display =
      Shell::Get()->display_manager()->FindDisplayContainingPoint(
          screen_location);

  if (!src_display.is_valid())
    return;

  const bool is_on_edge = IsOnBoundary(screen_location, src_display);

  // Restart the reset timer when the mouse moves off an edge.

  if (!is_on_edge) {
    if (current_state_ == DisplayAlignmentState::kOnEdge) {
      current_state_ = DisplayAlignmentState::kIdle;

      // The cursor was moved off the edge. Start the reset timer. If the cursor
      // does not hit an edge on the same display within |kCounterResetTime|,
      // state is reset by ResetState() and indicators will not be shown.
      action_trigger_timer_->Start(
          FROM_HERE, kCounterResetTime,
          base::BindOnce(&DisplayAlignmentController::ResetState,
                         base::Unretained(this)));
    }
    return;
  }

  if (current_state_ != DisplayAlignmentState::kIdle)
    return;

  // |trigger_count_| should only increment when the mouse hits the edges of
  // the same display.
  if (triggered_display_id_ == src_display.id()) {
    trigger_count_++;
  } else {
    triggered_display_id_ = src_display.id();
    trigger_count_ = 1;
  }

  action_trigger_timer_->Stop();
  current_state_ = DisplayAlignmentState::kOnEdge;

  if (trigger_count_ == kTriggerThresholdCount)
    ShowIndicators(src_display);
}

void DisplayAlignmentController::OnLockStateChanged(const bool locked) {
  is_locked_ = locked;
  RefreshState();
}

void DisplayAlignmentController::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  action_trigger_timer_ = std::move(timer);
}

const std::vector<std::unique_ptr<DisplayAlignmentIndicator>>&
DisplayAlignmentController::GetActiveIndicatorsForTesting() {
  return active_indicators_;
}

void DisplayAlignmentController::ShowIndicators(
    const display::Display& src_display) {
  DCHECK_EQ(src_display.id(), triggered_display_id_);

  current_state_ = DisplayAlignmentState::kIndicatorsVisible;

  // Iterate through all the active displays and see if they are neighbors to
  // |src_display|.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const display::Displays& display_list =
      display_manager->active_display_list();
  for (const display::Display& peer : display_list) {
    // Skip currently triggered display or it might be detected as a neighbor
    if (peer.id() == triggered_display_id_)
      continue;

    // Check whether |src_display| and |peer| are neighbors.
    gfx::Rect source_edge;
    gfx::Rect peer_edge;
    if (display::ComputeBoundary(src_display, peer, &source_edge, &peer_edge)) {
      // TODO(1070697): Handle pills overlapping for certain display
      // configuration.

      // Pills are created for the indicators in the src display, but not in the
      // peers.
      const std::string& dst_name =
          display_manager->GetDisplayInfo(peer.id()).name();
      active_indicators_.push_back(std::make_unique<DisplayAlignmentIndicator>(
          src_display, source_edge, dst_name));

      active_indicators_.push_back(std::make_unique<DisplayAlignmentIndicator>(
          peer, peer_edge, /*target_name=*/""));
    }
  }

  action_trigger_timer_->Start(
      FROM_HERE, kIndicatorVisibilityDuration,
      base::BindOnce(&DisplayAlignmentController::ResetState,
                     base::Unretained(this)));
}

void DisplayAlignmentController::ResetState() {
  action_trigger_timer_->Stop();
  active_indicators_.clear();
  trigger_count_ = 0;

  // Do not re-enable if disabled.
  if (current_state_ != DisplayAlignmentState::kDisabled)
    current_state_ = DisplayAlignmentState::kIdle;
}

void DisplayAlignmentController::RefreshState() {
  ResetState();

  // This feature is only enabled when the screen is not locked and there is
  // more than one display connected.
  if (is_locked_) {
    current_state_ = DisplayAlignmentState::kDisabled;
    return;
  }

  const display::Displays& display_list =
      Shell::Get()->display_manager()->active_display_list();
  if (display_list.size() < 2) {
    current_state_ = DisplayAlignmentState::kDisabled;
    return;
  }

  if (current_state_ == DisplayAlignmentState::kDisabled)
    current_state_ = DisplayAlignmentState::kIdle;
}

}  // namespace ash
