// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/search_metrics_reporter.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace local_search_service {
namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal =
    base::TimeDelta::FromMinutes(30);

// Prefs corresponding to IndexId values.
constexpr std::array<const char*, SearchMetricsReporter::kNumberIndexIds>
    kDailyCountPrefs = {
        prefs::kLocalSearchServiceMetricsCrosSettingsCount,
};

// Histograms corresponding to IndexId values.
constexpr std::array<const char*, SearchMetricsReporter::kNumberIndexIds>
    kDailyCountHistograms = {
        SearchMetricsReporter::kCrosSettingsName,
};

}  // namespace

constexpr char SearchMetricsReporter::kDailyEventIntervalName[];
constexpr char SearchMetricsReporter::kCrosSettingsName[];

constexpr int SearchMetricsReporter::kNumberIndexIds;

// This class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to SearchMetricsReporter.
class SearchMetricsReporter::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(SearchMetricsReporter* reporter)
      : reporter_(reporter) {
    DCHECK(reporter_);
  }

  ~DailyEventObserver() override = default;
  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  SearchMetricsReporter* reporter_;  // Not owned.
};

// static:
void SearchMetricsReporter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(
      registry, prefs::kLocalSearchServiceMetricsDailySample);
  for (const char* daily_count_pref : kDailyCountPrefs) {
    registry->RegisterIntegerPref(daily_count_pref, 0);
  }
}

SearchMetricsReporter::SearchMetricsReporter(
    PrefService* local_state_pref_service)
    : pref_service_(local_state_pref_service),
      daily_event_(std::make_unique<metrics::DailyEvent>(
          pref_service_,
          prefs::kLocalSearchServiceMetricsDailySample,
          kDailyEventIntervalName)) {
  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = pref_service_->GetInteger(kDailyCountPrefs[i]);
  }

  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

SearchMetricsReporter::~SearchMetricsReporter() = default;

void SearchMetricsReporter::SetIndexId(IndexId index_id) {
  DCHECK(!index_id_);
  index_id_ = index_id;
  DCHECK_LT(static_cast<size_t>(index_id), kDailyCountPrefs.size());
}

void SearchMetricsReporter::OnSearchPerformed() {
  DCHECK(index_id_);
  const size_t index = static_cast<size_t>(*index_id_);
  const char* daily_count_pref = kDailyCountPrefs[index];
  ++daily_counts_[index];
  pref_service_->SetInteger(daily_count_pref, daily_counts_[index]);
}

void SearchMetricsReporter::ReportDailyMetricsForTesting(
    metrics::DailyEvent::IntervalType type) {
  ReportDailyMetrics(type);
}

void SearchMetricsReporter::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  if (!index_id_)
    return;

  // Don't send metrics on first run or if the clock is changed.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    const size_t index = static_cast<size_t>(*index_id_);
    base::UmaHistogramCounts1000(kDailyCountHistograms[index],
                                 daily_counts_[index]);
  }

  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = 0;
    pref_service_->SetInteger(kDailyCountPrefs[i], 0);
  }
}

}  // namespace local_search_service
