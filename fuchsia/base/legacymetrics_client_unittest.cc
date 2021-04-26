// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl_test_base.h>
#include <string>
#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/metrics/histogram_macros.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "fuchsia/base/legacymetrics_client.h"
#include "fuchsia/base/legacymetrics_histogram_flattener.h"
#include "fuchsia/base/result_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cr_fuchsia {
namespace {

constexpr base::TimeDelta kReportInterval = base::TimeDelta::FromMinutes(1);

class TestMetricsRecorder
    : public fuchsia::legacymetrics::testing::MetricsRecorder_TestBase {
 public:
  TestMetricsRecorder() = default;
  ~TestMetricsRecorder() override = default;

  bool IsRecordInFlight() const { return ack_callback_.has_value(); }

  std::vector<fuchsia::legacymetrics::Event> WaitForEvents() {
    if (recorded_events_.empty()) {
      base::RunLoop run_loop;
      on_record_cb_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return std::move(recorded_events_);
  }

  void DropAck() { ack_callback_ = base::nullopt; }

  void SendAck() {
    (*ack_callback_)();
    ack_callback_ = base::nullopt;
  }

  void set_expect_ack_dropped(bool expect_dropped) {
    expect_ack_dropped_ = expect_dropped;
  }

  // fuchsia::legacymetrics::MetricsRecorder implementation.
  void Record(std::vector<fuchsia::legacymetrics::Event> events,
              RecordCallback callback) override {
    std::move(events.begin(), events.end(),
              std::back_inserter(recorded_events_));

    // Received a call to Record() before the previous one was acknowledged,
    // which can happen in some cases (e.g. flushing).
    if (ack_callback_)
      EXPECT_TRUE(expect_ack_dropped_);

    ack_callback_ = std::move(callback);

    if (on_record_cb_)
      std::move(on_record_cb_).Run();
  }

  void NotImplemented_(const std::string& name) override { FAIL() << name; }

 private:
  std::vector<fuchsia::legacymetrics::Event> recorded_events_;
  base::OnceClosure on_record_cb_;
  base::Optional<RecordCallback> ack_callback_;
  bool expect_ack_dropped_ = false;
};

class LegacyMetricsClientTest : public testing::Test {
 public:
  LegacyMetricsClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::IO) {}
  ~LegacyMetricsClientTest() override = default;

  void SetUp() override {
    service_binding_ =
        std::make_unique<base::fuchsia::ScopedSingleClientServiceBinding<
            fuchsia::legacymetrics::MetricsRecorder>>(
            test_context_.additional_services(), &test_recorder_);
    base::SetRecordActionTaskRunner(base::ThreadTaskRunnerHandle::Get());

    // Flush any dirty histograms from previous test runs in this process.
    GetLegacyMetricsDeltas();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::TestComponentContextForProcess test_context_;
  TestMetricsRecorder test_recorder_;
  std::unique_ptr<base::fuchsia::ScopedSingleClientServiceBinding<
      fuchsia::legacymetrics::MetricsRecorder>>
      service_binding_;
  LegacyMetricsClient client_;
};

TEST_F(LegacyMetricsClientTest, ReportIntervalBoundary) {
  client_.Start(kReportInterval);

  task_environment_.FastForwardBy(kReportInterval -
                                  base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
}

void PopulateAdditionalEvents(
    base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>
        callback) {
  fuchsia::legacymetrics::ImplementationDefinedEvent impl_event;
  impl_event.set_name("baz");

  fuchsia::legacymetrics::Event event;
  event.set_impl_defined_event(std::move(impl_event));

  std::vector<fuchsia::legacymetrics::Event> events;
  events.push_back(std::move(event));
  std::move(callback).Run(std::move(events));
}

TEST_F(LegacyMetricsClientTest, AllTypes) {
  client_.SetReportAdditionalMetricsCallback(
      base::BindRepeating(&PopulateAdditionalEvents));
  client_.Start(kReportInterval);

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  base::RecordComputedAction("bar");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(3u, events.size());
  EXPECT_EQ("baz", events[0].impl_defined_event().name());
  EXPECT_EQ("foo", events[1].histogram().name());
  EXPECT_EQ("bar", events[2].user_action_event().name());
}

TEST_F(LegacyMetricsClientTest, ReportSkippedNoEvents) {
  client_.Start(kReportInterval);

  // Verify that Record() is not invoked if there is no data to report.
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  // Add some events and allow the interval to lapse. Verify that the data is
  // reported.
  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();

  // Verify that Record() is skipped again for no-data.
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, MultipleReports) {
  client_.Start(kReportInterval);

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.SendAck();
}

TEST_F(LegacyMetricsClientTest, NoReportIfNeverAcked) {
  client_.Start(kReportInterval);

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
  test_recorder_.DropAck();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
}

TEST_F(LegacyMetricsClientTest, MetricsChannelDisconnected) {
  client_.Start(kReportInterval);
  service_binding_.reset();
  task_environment_.FastForwardBy(kReportInterval);
}

TEST_F(LegacyMetricsClientTest, Batching) {
  client_.Start(kReportInterval);

  // Log enough actions that the list will be split across multiple batches.
  // Batches are read out in reverse order, so even though it is being logged
  // first, it will be emitted in the final batch.
  base::RecordComputedAction("batch2");

  for (size_t i = 0; i < LegacyMetricsClient::kMaxBatchSize; ++i)
    base::RecordComputedAction("batch1");

  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  // First batch.
  auto events = test_recorder_.WaitForEvents();
  EXPECT_EQ(LegacyMetricsClient::kMaxBatchSize, events.size());
  for (const auto& event : events)
    EXPECT_EQ(event.user_action_event().name(), "batch1");
  test_recorder_.SendAck();

  // Second batch (remainder).
  events = test_recorder_.WaitForEvents();
  EXPECT_EQ(1u, events.size());
  for (const auto& event : events)
    EXPECT_EQ(event.user_action_event().name(), "batch2");
  test_recorder_.SendAck();
}

TEST_F(LegacyMetricsClientTest, FlushWithPending) {
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);

  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  service_binding_->events().OnCloseSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  // The service should be unbound once all data is drained.
  EXPECT_TRUE(service_binding_->has_clients());
  auto events = test_recorder_.WaitForEvents();
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("foo", events[0].histogram().name());
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, FlushNoData) {
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service_binding_->has_clients());
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  service_binding_->events().OnCloseSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, FlushWithOutstandingAck) {
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  // Send "foo", but don't ack.
  UMA_HISTOGRAM_COUNTS_1M("foo", 20);
  task_environment_.FastForwardBy(kReportInterval);
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());

  // Allow the flush operation to call Record() without waiting for a prior ack.
  test_recorder_.set_expect_ack_dropped(true);

  // Buffer another event and trigger a flush.
  UMA_HISTOGRAM_COUNTS_1M("bar", 20);
  EXPECT_TRUE(service_binding_->has_clients());
  service_binding_->events().OnCloseSoon();

  // Simulate an asynchronous ack from the recorder, which be delivered around
  // the same time as the flush's Record() call. The ack should be gracefully
  // ignored by the client.
  test_recorder_.SendAck();

  base::RunLoop().RunUntilIdle();

  auto events = test_recorder_.WaitForEvents();
  test_recorder_.SendAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ("foo", events[0].histogram().name());
  EXPECT_EQ("bar", events[1].histogram().name());
  EXPECT_FALSE(service_binding_->has_clients());
}

TEST_F(LegacyMetricsClientTest, ExternalFlushSignal) {
  ResultReceiver<base::OnceClosure> flush_receiver;
  client_.SetNotifyFlushCallback(flush_receiver.GetReceiveCallback());
  client_.Start(kReportInterval);
  base::RunLoop().RunUntilIdle();

  UMA_HISTOGRAM_COUNTS_1M("foo", 20);

  // Verify that reporting does not start until the flush completion callback is
  // run.
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());
  service_binding_->events().OnCloseSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_recorder_.IsRecordInFlight());

  // Verify that invoking the completion callback unblocks reporting.
  EXPECT_TRUE(flush_receiver.has_value());
  std::move(*flush_receiver).Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_recorder_.IsRecordInFlight());
}

}  // namespace
}  // namespace cr_fuchsia
