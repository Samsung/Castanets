// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/cpu_time_metrics.h"

#include <stdint.h>

#include <atomic>
#include <memory>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_observer.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"

namespace content {
namespace {

// Histogram macros expect an enum class with kMaxValue. Because
// content::ProcessType cannot be migrated to this style at the moment, we
// specify a separate version here. Keep in sync with content::ProcessType.
// TODO(eseckler): Replace with content::ProcessType after its migration.
enum class ProcessTypeForUma {
  kUnknown = 1,
  kBrowser,
  kRenderer,
  kPluginDeprecated,
  kWorkerDeprecated,
  kUtility,
  kZygote,
  kSandboxHelper,
  kGpu,
  kPpapiPlugin,
  kPpapiBroker,
  kMaxValue = kPpapiBroker,
};

static_assert(static_cast<int>(ProcessTypeForUma::kMaxValue) ==
                  PROCESS_TYPE_PPAPI_BROKER,
              "ProcessTypeForUma and CurrentProcessType() require updating");

ProcessTypeForUma CurrentProcessType() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (process_type.empty())
    return ProcessTypeForUma::kBrowser;
  if (process_type == switches::kRendererProcess)
    return ProcessTypeForUma::kRenderer;
  if (process_type == switches::kUtilityProcess)
    return ProcessTypeForUma::kUtility;
  if (process_type == switches::kSandboxIPCProcess)
    return ProcessTypeForUma::kSandboxHelper;
  if (process_type == switches::kGpuProcess)
    return ProcessTypeForUma::kGpu;
  if (process_type == switches::kPpapiPluginProcess)
    return ProcessTypeForUma::kPpapiPlugin;
  if (process_type == switches::kPpapiBrokerProcess)
    return ProcessTypeForUma::kPpapiBroker;
  NOTREACHED() << "Unexpected process type: " << process_type;
  return ProcessTypeForUma::kUnknown;
}

const char* GetPerThreadHistogramNameForProcessType(ProcessTypeForUma type) {
  switch (type) {
    case ProcessTypeForUma::kBrowser:
      return "Power.CpuTimeSecondsPerThreadType.Browser";
    case ProcessTypeForUma::kRenderer:
      return "Power.CpuTimeSecondsPerThreadType.Renderer";
    case ProcessTypeForUma::kGpu:
      return "Power.CpuTimeSecondsPerThreadType.GPU";
    default:
      return "Power.CpuTimeSecondsPerThreadType.Other";
  }
}

// Keep in sync with CpuTimeMetricsThreadType in
// //tools/metrics/histograms/enums.xml.
enum class CpuTimeMetricsThreadType {
  kUnattributedThread = 0,
  kOtherThread,
  kMainThread,
  kIOThread,
  kThreadPoolBackgroundWorkerThread,
  kThreadPoolForegroundWorkerThread,
  kThreadPoolServiceThread,
  kCompositorThread,
  kCompositorTileWorkerThread,
  kVizCompositorThread,
  kRendererUnspecifiedWorkerThread,
  kRendererDedicatedWorkerThread,
  kRendererSharedWorkerThread,
  kRendererAnimationAndPaintWorkletThread,
  kRendererServiceWorkerThread,
  kRendererAudioWorkletThread,
  kRendererFileThread,
  kRendererDatabaseThread,
  kRendererOfflineAudioRenderThread,
  kRendererReverbConvolutionBackgroundThread,
  kRendererHRTFDatabaseLoaderThread,
  kRendererAudioEncoderThread,
  kRendererVideoEncoderThread,
  kMemoryInfraThread,
  kSamplingProfilerThread,
  kNetworkServiceThread,
  kAudioThread,
  kInProcessUtilityThread,
  kInProcessRendererThread,
  kInProcessGpuThread,
  kMaxValue = kInProcessGpuThread,
};

CpuTimeMetricsThreadType GetThreadTypeFromName(const char* const thread_name) {
  if (!thread_name)
    return CpuTimeMetricsThreadType::kOtherThread;

  if (base::MatchPattern(thread_name, "Cr*Main")) {
    return CpuTimeMetricsThreadType::kMainThread;
  } else if (base::MatchPattern(thread_name, "Chrome*IOThread")) {
    return CpuTimeMetricsThreadType::kIOThread;
  } else if (base::MatchPattern(thread_name, "ThreadPool*Foreground*")) {
    return CpuTimeMetricsThreadType::kThreadPoolForegroundWorkerThread;
  } else if (base::MatchPattern(thread_name, "ThreadPool*Background*")) {
    return CpuTimeMetricsThreadType::kThreadPoolBackgroundWorkerThread;
  } else if (base::MatchPattern(thread_name, "ThreadPoolService*")) {
    return CpuTimeMetricsThreadType::kThreadPoolServiceThread;
  } else if (base::MatchPattern(thread_name, "Compositor")) {
    return CpuTimeMetricsThreadType::kCompositorThread;
  } else if (base::MatchPattern(thread_name, "CompositorTileWorker*")) {
    return CpuTimeMetricsThreadType::kCompositorTileWorkerThread;
  } else if (base::MatchPattern(thread_name, "VizCompositor*")) {
    return CpuTimeMetricsThreadType::kVizCompositorThread;
  } else if (base::MatchPattern(thread_name, "unspecified worker*")) {
    return CpuTimeMetricsThreadType::kRendererUnspecifiedWorkerThread;
  } else if (base::MatchPattern(thread_name, "DedicatedWorker*")) {
    return CpuTimeMetricsThreadType::kRendererDedicatedWorkerThread;
  } else if (base::MatchPattern(thread_name, "SharedWorker*")) {
    return CpuTimeMetricsThreadType::kRendererSharedWorkerThread;
  } else if (base::MatchPattern(thread_name, "AnimationWorklet*")) {
    return CpuTimeMetricsThreadType::kRendererAnimationAndPaintWorkletThread;
  } else if (base::MatchPattern(thread_name, "ServiceWorker*")) {
    return CpuTimeMetricsThreadType::kRendererServiceWorkerThread;
  } else if (base::MatchPattern(thread_name, "AudioWorklet*")) {
    return CpuTimeMetricsThreadType::kRendererAudioWorkletThread;
  } else if (base::MatchPattern(thread_name, "File thread")) {
    return CpuTimeMetricsThreadType::kRendererFileThread;
  } else if (base::MatchPattern(thread_name, "Database thread")) {
    return CpuTimeMetricsThreadType::kRendererDatabaseThread;
  } else if (base::MatchPattern(thread_name, "OfflineAudioRender*")) {
    return CpuTimeMetricsThreadType::kRendererOfflineAudioRenderThread;
  } else if (base::MatchPattern(thread_name, "Reverb convolution*")) {
    return CpuTimeMetricsThreadType::kRendererReverbConvolutionBackgroundThread;
  } else if (base::MatchPattern(thread_name, "HRTF*")) {
    return CpuTimeMetricsThreadType::kRendererHRTFDatabaseLoaderThread;
  } else if (base::MatchPattern(thread_name, "Audio encoder*")) {
    return CpuTimeMetricsThreadType::kRendererAudioEncoderThread;
  } else if (base::MatchPattern(thread_name, "Video encoder*")) {
    return CpuTimeMetricsThreadType::kRendererVideoEncoderThread;
  } else if (base::MatchPattern(thread_name, "MemoryInfra")) {
    return CpuTimeMetricsThreadType::kMemoryInfraThread;
  } else if (base::MatchPattern(thread_name, "StackSamplingProfiler")) {
    return CpuTimeMetricsThreadType::kSamplingProfilerThread;
  } else if (base::MatchPattern(thread_name, "NetworkService")) {
    return CpuTimeMetricsThreadType::kNetworkServiceThread;
  } else if (base::MatchPattern(thread_name, "AudioThread")) {
    return CpuTimeMetricsThreadType::kAudioThread;
  } else if (base::MatchPattern(thread_name, "Chrome_InProcUtilityThread")) {
    return CpuTimeMetricsThreadType::kInProcessUtilityThread;
  } else if (base::MatchPattern(thread_name, "Chrome_InProcRendererThread")) {
    return CpuTimeMetricsThreadType::kInProcessRendererThread;
  } else if (base::MatchPattern(thread_name, "Chrome_InProcGpuThread")) {
    return CpuTimeMetricsThreadType::kInProcessGpuThread;
  }

  // TODO(eseckler): Also break out Android's RenderThread here somehow?

  return CpuTimeMetricsThreadType::kOtherThread;
}

// Samples the process's CPU time after a specific number of task were executed
// on the current thread (process main). The number of tasks is a crude proxy
// for CPU activity within this process. We sample more frequently when the
// process is more active, thus ensuring we lose little CPU time attribution
// when the process is terminated, even after it was very active.
class ProcessCpuTimeTaskObserver : public base::TaskObserver {
 public:
  static ProcessCpuTimeTaskObserver* GetInstance() {
    static base::NoDestructor<ProcessCpuTimeTaskObserver> instance;
    return instance.get();
  }

  ProcessCpuTimeTaskObserver()
      : task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
             // TODO(eseckler): Consider hooking into process shutdown on
             // desktop to reduce metric data loss.
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        process_metrics_(base::ProcessMetrics::CreateCurrentProcessMetrics()),
        process_type_(CurrentProcessType()),
        // The observer is created on the main thread of the process.
        main_thread_id_(base::PlatformThread::CurrentId()) {
    // Browser and GPU processes have a longer lifetime (don't disappear between
    // navigations), and typically execute a large number of small main-thread
    // tasks. For these processes, choose a higher reporting interval.
    if (process_type_ == ProcessTypeForUma::kBrowser ||
        process_type_ == ProcessTypeForUma::kGpu) {
      reporting_interval_ = kReportAfterEveryNTasksPersistentProcess;
    } else {
      reporting_interval_ = kReportAfterEveryNTasksOtherProcess;
    }
    DETACH_FROM_SEQUENCE(thread_pool_);
  }

  // base::TaskObserver implementation:
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {}

  void DidProcessTask(const base::PendingTask& pending_task) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_);
    // We perform the collection from a background thread. Only schedule another
    // one after a reasonably large amount of work was executed after the last
    // collection completed. std::memory_order_relaxed because we only care that
    // we pick up the change back by the posted task eventually.
    if (collection_in_progress_.load(std::memory_order_relaxed))
      return;
    task_counter_++;
    if (task_counter_ == reporting_interval_) {
      // PostTask() applies a barrier, so this will be applied before the thread
      // pool task executes and sets |collection_in_progress_| back to false.
      collection_in_progress_.store(true, std::memory_order_relaxed);
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ProcessCpuTimeTaskObserver::CollectAndReportCpuTimeOnThreadPool,
              base::Unretained(this)));
      task_counter_ = 0;
    }
  }

  void CollectAndReportCpuTimeOnThreadPool() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(thread_pool_);

    // This might overflow. We only care that it is different for each cycle.
    current_cycle_++;

    // GetCumulativeCPUUsage() may return a negative value if sampling failed.
    base::TimeDelta cumulative_cpu_time =
        process_metrics_->GetCumulativeCPUUsage();
    base::TimeDelta cpu_time_delta = cumulative_cpu_time - reported_cpu_time_;
    if (cpu_time_delta > base::TimeDelta()) {
      UMA_HISTOGRAM_SCALED_ENUMERATION(
          "Power.CpuTimeSecondsPerProcessType", process_type_,
          cpu_time_delta.InMicroseconds(), base::Time::kMicrosecondsPerSecond);
      reported_cpu_time_ = cumulative_cpu_time;
    }

    // Also report a breakdown by thread type.
    base::TimeDelta unattributed_delta = cpu_time_delta;
    if (process_metrics_->GetCumulativeCPUUsagePerThread(
            cumulative_thread_times_)) {
      for (const auto& entry : cumulative_thread_times_) {
        base::PlatformThreadId tid = entry.first;
        base::TimeDelta cumulative_time = entry.second;

        auto it_and_inserted = thread_details_.emplace(
            tid, ThreadDetails{base::TimeDelta(), current_cycle_});
        ThreadDetails* thread_details = &it_and_inserted.first->second;

        if (it_and_inserted.second) {
          // New thread.
          thread_details->type = GuessThreadType(tid);
        }

        thread_details->last_updated_cycle = current_cycle_;

        // Skip negative or null values, might be a transient collection error.
        if (cumulative_time <= base::TimeDelta())
          continue;

        if (cumulative_time < thread_details->reported_cpu_time) {
          // PlatformThreadId was likely reused, reset the details.
          thread_details->reported_cpu_time = base::TimeDelta();
          thread_details->type = GuessThreadType(tid);
        }

        base::TimeDelta thread_delta =
            cumulative_time - thread_details->reported_cpu_time;
        unattributed_delta -= thread_delta;

        ReportThreadCpuTimeDelta(thread_details->type, thread_delta);
        thread_details->reported_cpu_time = cumulative_time;
      }

      // Erase tracking for threads that have disappeared, as their
      // PlatformThreadId may be reused later.
      for (auto it = thread_details_.begin(); it != thread_details_.end();) {
        if (it->second.last_updated_cycle == current_cycle_) {
          it++;
        } else {
          it = thread_details_.erase(it);
        }
      }
    }

    // Report the difference of the process's total CPU time and all thread's
    // CPU time as unattributed time (e.g. time consumed by threads that died).
    if (unattributed_delta > base::TimeDelta()) {
      ReportThreadCpuTimeDelta(CpuTimeMetricsThreadType::kUnattributedThread,
                               unattributed_delta);
    }

    collection_in_progress_.store(false, std::memory_order_relaxed);
  }

 private:
  struct ThreadDetails {
    base::TimeDelta reported_cpu_time;
    uint32_t last_updated_cycle = 0;
    CpuTimeMetricsThreadType type = CpuTimeMetricsThreadType::kOtherThread;
  };

  void ReportThreadCpuTimeDelta(CpuTimeMetricsThreadType type,
                                base::TimeDelta cpu_time_delta) {
    // Histogram name cannot change after being used once. That's ok since this
    // only depends on the process type, which also doesn't change.
    static const char* histogram_name =
        GetPerThreadHistogramNameForProcessType(process_type_);
    UMA_HISTOGRAM_SCALED_ENUMERATION(histogram_name, type,
                                     cpu_time_delta.InMicroseconds(),
                                     base::Time::kMicrosecondsPerSecond);
  }

  CpuTimeMetricsThreadType GuessThreadType(base::PlatformThreadId tid) {
    // Match the main thread by TID, so that this also works for WebView, where
    // the main thread can have an arbitrary name.
    if (tid == main_thread_id_)
      return CpuTimeMetricsThreadType::kMainThread;
    const char* name = base::ThreadIdNameManager::GetInstance()->GetName(tid);
    return GetThreadTypeFromName(name);
  }

  // Sample CPU time after a certain number of main-thread task to balance
  // overhead of sampling and loss at process termination.
  static constexpr int kReportAfterEveryNTasksPersistentProcess = 500;
  static constexpr int kReportAfterEveryNTasksOtherProcess = 100;

  // Accessed on main thread.
  SEQUENCE_CHECKER(main_thread_);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int task_counter_ = 0;
  int reporting_interval_ = 0;  // set in constructor.

  // Accessed on |task_runner_|.
  SEQUENCE_CHECKER(thread_pool_);
  uint32_t current_cycle_ = 0;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
  ProcessTypeForUma process_type_;
  base::PlatformThreadId main_thread_id_;
  base::TimeDelta reported_cpu_time_;
  // Stored as instance variable to avoid allocation churn.
  base::ProcessMetrics::CPUUsagePerThread cumulative_thread_times_;
  base::flat_map<base::PlatformThreadId, ThreadDetails> thread_details_;

  // Accessed on both sequences.
  std::atomic<bool> collection_in_progress_;
};

}  // namespace

void SetupCpuTimeMetrics() {
  // May be called multiple times for in-process renderer/utility/GPU processes.
  static bool did_setup = false;
  if (did_setup)
    return;
  base::MessageLoopCurrent::Get()->AddTaskObserver(
      ProcessCpuTimeTaskObserver::GetInstance());
  did_setup = true;
}

void SampleCpuTimeMetricsForTesting() {
  ProcessCpuTimeTaskObserver::GetInstance()
      ->CollectAndReportCpuTimeOnThreadPool();
}

}  // namespace content
