// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"

#include "base/check_op.h"

// static
constexpr base::TimeDelta ThumbnailCaptureDriver::kCooldownDelay;

// static
constexpr size_t ThumbnailCaptureDriver::kMaxCooldownRetries;

ThumbnailCaptureDriver::ThumbnailCaptureDriver(Client* client)
    : client_(client) {}

ThumbnailCaptureDriver::~ThumbnailCaptureDriver() = default;

void ThumbnailCaptureDriver::UpdatePageReadiness(PageReadiness page_readiness) {
  page_readiness_ = page_readiness;
  UpdateCaptureState();
}

void ThumbnailCaptureDriver::UpdatePageVisibility(bool page_visible) {
  page_visible_ = page_visible;
  UpdateCaptureState();
}

void ThumbnailCaptureDriver::UpdateThumbnailVisibility(bool thumbnail_visible) {
  thumbnail_visible_ = thumbnail_visible;
  UpdateCaptureState();
}

void ThumbnailCaptureDriver::SetCanCapture(bool can_capture) {
  can_capture_ = can_capture;
  UpdateCaptureState();
}

void ThumbnailCaptureDriver::GotFrame() {
  if (capture_state_ == CaptureState::kCooldown)
    captured_cooldown_frame_ = true;
}

void ThumbnailCaptureDriver::UpdateCaptureState() {
  // Stop any existing capture and return if the page is not ready.
  if (page_readiness_ == PageReadiness::kNotReady) {
    client_->StopCapture();
    capture_state_ = CaptureState::kNoCapture;
    return;
  }

  // Don't capture when the page is visible and the thumbnail is not
  // requested.
  if (!thumbnail_visible_ && page_visible_) {
    client_->StopCapture();
    if (capture_state_ < CaptureState::kHaveFinalCapture)
      capture_state_ = CaptureState::kNoCapture;
    return;
  }

  // For now don't force-load background pages. This is not ideal. We would
  // like to grab frames from background pages to make hover cards and the
  // "Mohnstrudel" touch/tablet tabstrip more responsive by pre-loading
  // thumbnails from those pages. However, this currently results in a number
  // of test failures and a possible violation of an assumption made by the
  // renderer. TODO(crbug.com/1073141): Figure out how to force-render
  // background tabs. This bug has detailed descriptions of steps we might
  // take to make capture more flexible in this area.
  if (!thumbnail_visible_ && !page_visible_) {
    client_->StopCapture();
    if (capture_state_ < CaptureState::kHaveFinalCapture)
      capture_state_ = CaptureState::kNoCapture;
    return;
  }

  // Now we know the page is ready for capture and the thumbnail is
  // visible.

  // If the page is in its final state and we already have a good
  // thumbnail, don't need to anything.
  if (page_readiness_ == PageReadiness::kReadyForFinalCapture &&
      capture_state_ == CaptureState::kHaveFinalCapture) {
    return;
  }

  // Now we know the page is a candidate for capture.

  // Request to capture if we haven't done so.
  if (capture_state_ < CaptureState::kCaptureRequested) {
    capture_state_ = CaptureState::kCaptureRequested;
    client_->RequestCapture();
  }

  // Now, our |capture_state_| is at least |CaptureState::kCaptureRequested|.

  // Wait until our client is able to capture.
  if (!can_capture_) {
    // It is possible we were actively capturing and the client reported
    // it can no longer capture. Reset our state to re-request capture
    // later.
    capture_state_ = CaptureState::kCaptureRequested;
    cooldown_timer_.AbandonAndStop();
    return;
  }

  // The client is ready so start capturing. Continue below in case the
  // page is fully loaded, in which case we will wrap things up
  // immediately.
  if (capture_state_ == CaptureState::kCaptureRequested) {
    capture_state_ = CaptureState::kCapturing;
    client_->StartCapture();
  }

  // If the page is finalized, enter cooldown if we haven't yet.
  if (page_readiness_ == PageReadiness::kReadyForFinalCapture &&
      capture_state_ == CaptureState::kCapturing) {
    StartCooldown();
    return;
  }

  // If the page is finalized and we are in cooldown capture mode, we
  // don't need to do anything. The cooldown timer callback will
  // finalize everything.
  if (page_readiness_ == PageReadiness::kReadyForFinalCapture &&
      capture_state_ == CaptureState::kCooldown) {
    return;
  }

  // If we aren't actively capturing, we should've handled this above.
  DCHECK_EQ(capture_state_, CaptureState::kCapturing)
      << "page_readiness_ = " << static_cast<int>(page_readiness_);
}

void ThumbnailCaptureDriver::StartCooldown() {
  DCHECK_EQ(page_readiness_, PageReadiness::kReadyForFinalCapture);
  DCHECK_EQ(capture_state_, CaptureState::kCapturing);

  capture_state_ = CaptureState::kCooldown;
  captured_cooldown_frame_ = false;
  cooldown_retry_count_ = 0U;

  if (cooldown_timer_.IsRunning()) {
    cooldown_timer_.Reset();
  } else {
    cooldown_timer_.Start(
        FROM_HERE, kCooldownDelay,
        base::BindRepeating(&ThumbnailCaptureDriver::OnCooldownEnded,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ThumbnailCaptureDriver::OnCooldownEnded() {
  if (capture_state_ < CaptureState::kCooldown)
    return;

  if (!captured_cooldown_frame_ &&
      cooldown_retry_count_ < kMaxCooldownRetries) {
    ++cooldown_retry_count_;
    cooldown_timer_.Reset();
    return;
  }

  capture_state_ = CaptureState::kHaveFinalCapture;
  client_->StopCapture();
}
