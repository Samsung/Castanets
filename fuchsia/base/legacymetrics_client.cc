// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/legacymetrics_client.h"

#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "fuchsia/base/legacymetrics_histogram_flattener.h"

namespace cr_fuchsia {

constexpr size_t LegacyMetricsClient::kMaxBatchSize;

LegacyMetricsClient::LegacyMetricsClient() = default;

LegacyMetricsClient::~LegacyMetricsClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LegacyMetricsClient::Start(base::TimeDelta report_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(report_interval, base::TimeDelta::FromSeconds(0));
  DCHECK(!metrics_recorder_) << "Start() called more than once.";

  report_interval_ = report_interval;
  metrics_recorder_ = base::ComponentContextForProcess()
                          ->svc()
                          ->Connect<fuchsia::legacymetrics::MetricsRecorder>();
  metrics_recorder_.set_error_handler(fit::bind_member(
      this, &LegacyMetricsClient::OnMetricsRecorderDisconnected));
  metrics_recorder_.events().OnCloseSoon =
      fit::bind_member(this, &LegacyMetricsClient::OnCloseSoon);
  user_events_recorder_ = std::make_unique<LegacyMetricsUserActionRecorder>();
  ScheduleNextReport();
}

void LegacyMetricsClient::SetReportAdditionalMetricsCallback(
    ReportAdditionalMetricsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!metrics_recorder_)
      << "SetReportAdditionalMetricsCallback() must be called before Start().";
  DCHECK(!report_additional_callback_);
  DCHECK(callback);

  report_additional_callback_ = std::move(callback);
}

void LegacyMetricsClient::SetNotifyFlushCallback(NotifyFlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  DCHECK(!metrics_recorder_)
      << "SetNotifyFlushCallback() must be called before Start().";

  notify_flush_callback_ = std::move(callback);
}

void LegacyMetricsClient::ScheduleNextReport() {
  DCHECK(!is_flushing_);

  DVLOG(1) << "Scheduling next report in " << report_interval_.InSeconds()
           << "seconds.";
  timer_.Start(FROM_HERE, report_interval_, this,
               &LegacyMetricsClient::StartReport);
}

void LegacyMetricsClient::StartReport() {
  if (!report_additional_callback_) {
    Report({});
    return;
  }
  report_additional_callback_.Run(
      base::BindOnce(&LegacyMetricsClient::Report, weak_factory_.GetWeakPtr()));
}

void LegacyMetricsClient::Report(
    std::vector<fuchsia::legacymetrics::Event> events) {
  DCHECK(metrics_recorder_);
  DVLOG(1) << __func__ << " called.";

  // Include histograms.
  for (auto& histogram : GetLegacyMetricsDeltas()) {
    fuchsia::legacymetrics::Event histogram_event;
    histogram_event.set_histogram(std::move(histogram));
    events.push_back(std::move(histogram_event));
  }

  // Include user events.
  if (user_events_recorder_->HasEvents()) {
    for (auto& event : user_events_recorder_->TakeEvents()) {
      fuchsia::legacymetrics::Event user_event;
      user_event.set_user_action_event(std::move(event));
      events.push_back(std::move(user_event));
    }
  }

  std::move(events.begin(), events.end(), std::back_inserter(to_send_));

  DrainBuffer();
}

void LegacyMetricsClient::DrainBuffer() {
  DVLOG(1) << __func__ << " called.";

  if (record_ack_pending_) {
    // There is a Record() call already inflight. When it is acknowledged,
    // buffer draining will continue.
    return;
  }

  if (to_send_.empty()) {
    DVLOG(1) << "Buffer drained.";

    if (is_flushing_) {
      metrics_recorder_.Unbind();
    } else {
      ScheduleNextReport();
    }

    return;
  }

  // Since ordering doesn't matter, we can efficiently drain |to_send_| by
  // repeatedly sending and truncating its tail.
  const size_t batch_size = std::min(to_send_.size(), kMaxBatchSize);
  const size_t batch_start_idx = to_send_.size() - batch_size;
  std::vector<fuchsia::legacymetrics::Event> batch;
  batch.resize(batch_size);
  std::move(to_send_.begin() + batch_start_idx, to_send_.end(), batch.begin());
  to_send_.resize(to_send_.size() - batch_size);

  record_ack_pending_ = true;
  metrics_recorder_->Record(std::move(batch), [this]() {
    record_ack_pending_ = false;
    DrainBuffer();
  });
}

void LegacyMetricsClient::OnMetricsRecorderDisconnected(zx_status_t status) {
  ZX_LOG(ERROR, status) << "MetricsRecorder connection lost.";

  // Stop recording & reporting user events.
  user_events_recorder_.reset();
  timer_.AbandonAndStop();
}

void LegacyMetricsClient::OnCloseSoon() {
  DVLOG(1) << __func__ << " called.";

  timer_.AbandonAndStop();

  is_flushing_ = true;
  if (notify_flush_callback_) {
    // Defer reporting until the flush operation has finished.
    std::move(notify_flush_callback_)
        .Run(base::BindOnce(&LegacyMetricsClient::StartReport,
                            weak_factory_.GetWeakPtr()));
  } else {
    StartReport();
  }
}

}  // namespace cr_fuchsia
