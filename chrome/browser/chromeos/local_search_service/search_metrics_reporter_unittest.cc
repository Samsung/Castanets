// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/search_metrics_reporter.h"

#include <memory>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/local_search_service/shared_structs.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class SearchMetricsReporterTest : public testing::Test {
 public:
  SearchMetricsReporterTest() = default;
  ~SearchMetricsReporterTest() override = default;

  void SetUp() override {
    SearchMetricsReporter::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void TearDown() override { reporter_.reset(); }

 protected:
  void SetReporter(IndexId index_id) {
    reporter_ = std::make_unique<SearchMetricsReporter>(&pref_service_);
    reporter_->SetIndexId(index_id);
  }

  // Notifies |reporter_| that a search is performed.
  void SendOnSearchPerformed() { reporter_->OnSearchPerformed(); }

  // Instructs |reporter_| to report daily metrics for reason |type|.
  void TriggerDailyEvent(metrics::DailyEvent::IntervalType type) {
    reporter_->ReportDailyMetricsForTesting(type);
  }

  // Instructs |reporter_| to report daily metrics due to the passage of a day
  // and verifies that it reports one sample with each of the passed values.
  void TriggerDailyEventAndVerifyHistograms(const std::string& histogram_name,
                                            int expected_count) {
    base::HistogramTester histogram_tester;

    TriggerDailyEvent(metrics::DailyEvent::IntervalType::DAY_ELAPSED);
    histogram_tester.ExpectUniqueSample(histogram_name, expected_count, 1);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SearchMetricsReporter> reporter_;
};

TEST_F(SearchMetricsReporterTest, CountAndReportEvents) {
  SetReporter(IndexId::kCrosSettings);
  SendOnSearchPerformed();
  SendOnSearchPerformed();
  SendOnSearchPerformed();
  TriggerDailyEventAndVerifyHistograms(SearchMetricsReporter::kCrosSettingsName,
                                       3);

  // The next day, another two searches.
  SendOnSearchPerformed();
  SendOnSearchPerformed();
  TriggerDailyEventAndVerifyHistograms(SearchMetricsReporter::kCrosSettingsName,
                                       2);
}

TEST_F(SearchMetricsReporterTest, LoadInitialCountsFromPrefs) {
  // Create a new reporter and check that it loads its initial event counts from
  // prefs.
  pref_service_.SetInteger(prefs::kLocalSearchServiceMetricsCrosSettingsCount,
                           2);
  SetReporter(IndexId::kCrosSettings);

  TriggerDailyEventAndVerifyHistograms(SearchMetricsReporter::kCrosSettingsName,
                                       2);

  // The previous report should've cleared the prefs, so a new reporter should
  // start out at zero.
  TriggerDailyEventAndVerifyHistograms(SearchMetricsReporter::kCrosSettingsName,
                                       0);
}

TEST_F(SearchMetricsReporterTest, IgnoreDailyEventFirstRun) {
  SetReporter(IndexId::kCrosSettings);
  // metrics::DailyEvent notifies observers immediately on first run. Histograms
  // shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::FIRST_RUN);
  tester.ExpectTotalCount(SearchMetricsReporter::kCrosSettingsName, 0);
}

TEST_F(SearchMetricsReporterTest, IgnoreDailyEventClockChanged) {
  SetReporter(IndexId::kCrosSettings);
  SendOnSearchPerformed();

  // metrics::DailyEvent notifies observers if it sees that the system clock has
  // jumped back. Histograms shouldn't be sent in this case.
  base::HistogramTester tester;
  TriggerDailyEvent(metrics::DailyEvent::IntervalType::CLOCK_CHANGED);
  tester.ExpectTotalCount(SearchMetricsReporter::kCrosSettingsName, 0);

  // The existing stats should be cleared when the clock change notification is
  // received, so the next report should only contain zeros.
  TriggerDailyEventAndVerifyHistograms(SearchMetricsReporter::kCrosSettingsName,
                                       0);
}

}  // namespace local_search_service
