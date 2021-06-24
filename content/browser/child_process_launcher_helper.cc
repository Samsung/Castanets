// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/platform/platform_channel.h"

#if defined(OS_ANDROID)
#include "content/browser/android/launcher_thread.h"
#endif

#if defined(CASTANETS)
#include "base/base_switches.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"

int kTcpConnectionTimeout = 5;
#endif

namespace content {
namespace internal {

namespace {

void RecordHistogramsOnLauncherThread(base::TimeDelta launch_time) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Log the launch time, separating out the first one (which will likely be
  // slower due to the rest of the browser initializing at the same time).
  static bool done_first_launch = false;
  if (done_first_launch) {
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchSubsequent", launch_time);
  } else {
    UMA_HISTOGRAM_TIMES("MPArch.ChildProcessLaunchFirst", launch_time);
    done_first_launch = true;
  }
}

}  // namespace

ChildProcessLauncherHelper::Process::Process(Process&& other)
    : process(std::move(other.process))
#if BUILDFLAG(USE_ZYGOTE_HANDLE)
      ,
      zygote(other.zygote)
#endif
{
}

ChildProcessLauncherHelper::Process&
ChildProcessLauncherHelper::Process::Process::operator=(
    ChildProcessLauncherHelper::Process&& other) {
  DCHECK_NE(this, &other);
  process = std::move(other.process);
#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  zygote = other.zygote;
#endif
  return *this;
}

ChildProcessLauncherHelper::ChildProcessLauncherHelper(
    int child_process_id,
    std::unique_ptr<base::CommandLine> command_line,
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
    bool terminate_on_shutdown,
#if defined(OS_ANDROID)
    bool can_use_warm_up_connection,
#endif
    mojo::OutgoingInvitation mojo_invitation,
    const mojo::ProcessErrorCallback& process_error_callback,
    std::map<std::string, base::FilePath> files_to_preload)
    : child_process_id_(child_process_id),
      client_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      command_line_(std::move(command_line)),
      delegate_(std::move(delegate)),
      child_process_launcher_(child_process_launcher),
#if defined(CASTANETS)
      tcp_connected_(false),
#endif
      terminate_on_shutdown_(terminate_on_shutdown),
      mojo_invitation_(std::move(mojo_invitation)),
      process_error_callback_(process_error_callback),
      files_to_preload_(std::move(files_to_preload))
#if defined(OS_ANDROID)
      ,
      can_use_warm_up_connection_(can_use_warm_up_connection)
#endif
{
#if defined(CASTANETS)
  tcp_success_callback_ = base::BindRepeating(
      &ChildProcessLauncherHelper::OnCastanetsRendererLaunchedViaTcp,
      base::Unretained(this));
  relaunch_renderer_process_monitor_timeout_.reset(new TimeoutMonitor(
      base::Bind(&ChildProcessLauncherHelper::OnCastanetsRendererTimeout,
                 base::Unretained(this))));
  relaunch_renderer_process_monitor_timeout_->Start(
      base::TimeDelta::FromSeconds(kTcpConnectionTimeout));
#endif
}

ChildProcessLauncherHelper::~ChildProcessLauncherHelper() = default;

#if defined(CASTANETS)
void ChildProcessLauncherHelper::OnCastanetsRendererTimeout() {
  success_or_timeout_event_.Signal();
}
void ChildProcessLauncherHelper::OnCastanetsRendererLaunchedViaTcp() {
  tcp_connected_ = true;
  success_or_timeout_event_.Signal();
  relaunch_renderer_process_monitor_timeout_->Stop();
}
#endif

void ChildProcessLauncherHelper::StartLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());

  BeforeLaunchOnClientThread();

#if defined(OS_FUCHSIA)
  mojo_channel_.emplace();
#else   // !defined(OS_FUCHSIA)
  mojo_named_channel_ = CreateNamedPlatformChannelOnClientThread();
  if (!mojo_named_channel_)
    mojo_channel_.emplace();
#endif  //  !defined(OS_FUCHSIA)

  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChildProcessLauncherHelper::LaunchOnLauncherThread,
                     this));
}

void ChildProcessLauncherHelper::LaunchOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  begin_launch_time_ = base::TimeTicks::Now();

  std::unique_ptr<FileMappedForLaunch> files_to_register = GetFilesToMap();

  bool is_synchronous_launch = true;
  int launch_result = LAUNCH_RESULT_FAILURE;
  base::LaunchOptions options;

  Process process;
  if (BeforeLaunchOnLauncherThread(*files_to_register, &options)) {
    process =
        LaunchProcessOnLauncherThread(options, std::move(files_to_register),
#if defined(OS_ANDROID)
                                      can_use_warm_up_connection_,
#endif
                                      &is_synchronous_launch, &launch_result);

    AfterLaunchOnLauncherThread(process, options);
  }

  if (is_synchronous_launch) {
    PostLaunchOnLauncherThread(std::move(process), launch_result);
  }
}
#if defined(CASTANETS)
ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::RetrySendOutgoingInvitation(
    base::ProcessHandle old_process,
    const mojo::ProcessErrorCallback& error_callback) {
  command_line_->AppendSwitchASCII(switches::kRendererClientId,
                                   std::to_string(child_process_id_));

  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  mojo_named_channel_.reset();
  mojo_channel_.emplace();

  begin_launch_time_ = base::TimeTicks::Now();

  std::unique_ptr<FileMappedForLaunch> files_to_register = GetFilesToMap();

  int launch_result = LAUNCH_RESULT_FAILURE;
  base::LaunchOptions options;

  Process process;
  if (BeforeLaunchOnLauncherThread(*files_to_register, &options)) {
    process.process = base::LaunchProcess(*command_line(), options);
    launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                              : LAUNCH_RESULT_FAILURE;

    AfterLaunchOnLauncherThread(process, options);
  }

  mojo::OutgoingInvitation::Retry(old_process, process.process.Handle(),
                                  mojo_channel_->TakeLocalEndpoint());

  return process;
}
#endif
void ChildProcessLauncherHelper::PostLaunchOnLauncherThread(
    ChildProcessLauncherHelper::Process process,
    int launch_result) {
#if defined(CASTANETS)
  // If mojo_named_channel_ is valid, we are trying to launch process
  // in Castanets mode, mojo_channel_ is no longer needed.
  if (mojo_named_channel_)
    mojo_channel_ = base::nullopt;
#endif
  if (mojo_channel_)
    mojo_channel_->RemoteProcessLaunchAttempted();

  if (process.process.IsValid()) {
    RecordHistogramsOnLauncherThread(base::TimeTicks::Now() -
                                     begin_launch_time_);
  }

  // Take ownership of the broker client invitation here so it's destroyed when
  // we go out of scope regardless of the outcome below.
  mojo::OutgoingInvitation invitation = std::move(mojo_invitation_);
  if (process.process.IsValid()) {
#if !defined(OS_FUCHSIA)
    if (mojo_named_channel_) {
      DCHECK(!mojo_channel_);
      mojo::OutgoingInvitation::Send(
          std::move(invitation), process.process.Handle(),
          mojo_named_channel_->TakeServerEndpoint(), process_error_callback_);
    } else
#endif
    // Set up Mojo IPC to the new process.
    {
      DCHECK(mojo_channel_);
      DCHECK(mojo_channel_->local_endpoint().is_valid());
      mojo::OutgoingInvitation::Send(
          std::move(invitation), process.process.Handle(),
          mojo_channel_->TakeLocalEndpoint(), process_error_callback_);
    }
  }

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ChildProcessLauncherHelper::PostLaunchOnClientThread,
                     this, std::move(process), launch_result));
}

void ChildProcessLauncherHelper::PostLaunchOnClientThread(
    ChildProcessLauncherHelper::Process process,
    int error_code) {
  if (child_process_launcher_) {
    child_process_launcher_->Notify(std::move(process), error_code);
  } else if (process.process.IsValid() && terminate_on_shutdown_) {
    // Client is gone, terminate the process.
    ForceNormalProcessTerminationAsync(std::move(process));
  }
}

std::string ChildProcessLauncherHelper::GetProcessType() {
  return command_line()->GetSwitchValueASCII(switches::kProcessType);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
    ChildProcessLauncherHelper::Process process) {
  if (CurrentlyOnProcessLauncherTaskRunner()) {
    ForceNormalProcessTerminationSync(std::move(process));
    return;
  }
  // On Posix, EnsureProcessTerminated can lead to 2 seconds of sleep!
  // So don't do this on the UI/IO threads.
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::ForceNormalProcessTerminationSync,
          std::move(process)));
}

}  // namespace internal

// static
base::SingleThreadTaskRunner* GetProcessLauncherTaskRunner() {
#if defined(OS_ANDROID)
  // Android specializes Launcher thread so it is accessible in java.
  // Note Android never does clean shutdown, so shutdown use-after-free
  // concerns are not a problem in practice.
  // This process launcher thread will use the Java-side process-launching
  // thread, instead of creating its own separate thread on C++ side. Note
  // that means this thread will not be joined on shutdown, and may cause
  // use-after-free if anything tries to access objects deleted by
  // AtExitManager, such as non-leaky LazyInstance.
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>>
      launcher_task_runner(android::LauncherThread::GetTaskRunner());
  return (*launcher_task_runner).get();
#else   // defined(OS_ANDROID)
  // TODO(http://crbug.com/820200): Investigate whether we could use
  // SequencedTaskRunner on platforms other than Windows.
  static base::LazyThreadPoolSingleThreadTaskRunner launcher_task_runner =
      LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
          base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                           base::TaskShutdownBehavior::BLOCK_SHUTDOWN),
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  return launcher_task_runner.Get().get();
#endif  // defined(OS_ANDROID)
}

// static
bool CurrentlyOnProcessLauncherTaskRunner() {
  return GetProcessLauncherTaskRunner()->RunsTasksInCurrentSequence();
}

}  // namespace content
