// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/sandbox_host_linux.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/zygote/common/common_sandbox_support_linux.h"
#include "services/service_manager/zygote/common/zygote_handle.h"
#include "services/service_manager/zygote/host/zygote_communication_linux.h"
#include "services/service_manager/zygote/host/zygote_host_impl_linux.h"

#if defined(CASTANETS)
#include "base/base_switches.h"
#include "base/distributed_chromium_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#endif

namespace content {
namespace internal {

base::Optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
#if defined(CASTANETS)
  if (base::Castanets::IsEnabled()) {
    if (!remote_process_)
      return base::nullopt;

    if (base::Castanets::ServerAddress().empty()) {
      mojo::NamedPlatformChannel::Options options;
      options.port = (GetProcessType() == switches::kRendererProcess)
                         ? mojo::kCastanetsRendererPort
                         : mojo::kCastanetsUtilityPort;

      // This socket pair is not used, however it is added
      // to avoid failure of validation check of codes afterwards.
      mojo_channel_.emplace();
      return mojo::NamedPlatformChannel(options);
    }
    return base::nullopt;
  } else {
    return base::nullopt;
  }
#else
  return base::nullopt;
#endif
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  DCHECK_CURRENTLY_ON(client_thread_id_);
#if defined(CASTANETS)
  // Request discovery client to run renderer process on the remote node.
  if (GetProcessType() == switches::kRendererProcess && remote_process_) {
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::SHARED;
    scoped_refptr<dbus::Bus> bus = new dbus::Bus(bus_options);

    dbus::ObjectProxy* object_proxy =
        bus->GetObjectProxy("discovery.client.listener",
                            dbus::ObjectPath("/discovery/client/object"));

    if (!object_proxy) {
      LOG(ERROR) << "Fail to get object proxy.";
      return;
    }

    dbus::MethodCall method_call("discovery.client.interface", "RunService");
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(command_line()->argv());

    std::unique_ptr<dbus::Response> response(object_proxy->CallMethodAndBlock(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));

    if (response.get()) {
      bool stat;
      dbus::MessageReader reader(response.get());
      reader.PopBool(&stat);
      if (stat) {
        LOG(INFO) << "Success to run renderer process on the remote node.";
      } else {
        LOG(ERROR) << "Fail to run renderer process on the remote node.";
      }
    } else {
      LOG(ERROR) << "Fail to run renderer process on the remote node.";
    }
    bus->ShutdownAndBlock();
  }
#endif
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return CreateDefaultPosixFilesToMap(child_process_id(),
                                      mojo_channel_->remote_endpoint(),
                                      true /* include_service_required_files */,
                                      GetProcessType(), command_line());
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  // Convert FD mapping to FileHandleMappingVector
  options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
      base::GlobalDescriptors::kBaseDescriptor);

  if (GetProcessType() == switches::kRendererProcess) {
    const int sandbox_fd = SandboxHostLinux::GetInstance()->GetChildSocket();
    options->fds_to_remap.push_back(
        std::make_pair(sandbox_fd, service_manager::GetSandboxFD()));
  }

  options->environment = delegate_->GetEnvironment();

  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;

  service_manager::ZygoteHandle zygote_handle =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoZygote)
          ? nullptr
          : delegate_->GetZygote();
  if (zygote_handle) {
    // TODO(crbug.com/569191): If chrome supported multiple zygotes they could
    // be created lazily here, or in the delegate GetZygote() implementations.
    // Additionally, the delegate could provide a UseGenericZygote() method.
    base::ProcessHandle handle = zygote_handle->ForkRequest(
        command_line()->argv(), files_to_register->GetMapping(),
        GetProcessType());
    *launch_result = LAUNCH_RESULT_SUCCESS;

#if !defined(OS_OPENBSD)
    if (handle) {
      // This is just a starting score for a renderer or extension (the
      // only types of processes that will be started this way).  It will
      // get adjusted as time goes on.  (This is the same value as
      // chrome::kLowestRendererOomScore in chrome/chrome_constants.h, but
      // that's not something we can include here.)
      const int kLowestRendererOomScore = 300;
      service_manager::ZygoteHostImpl::GetInstance()->AdjustRendererOOMScore(
          handle, kLowestRendererOomScore);
    }
#endif

    Process process;
    process.process = base::Process(handle);
    process.zygote = zygote_handle;
    return process;
  }

#if defined(CASTANETS)
  if (remote_process_) {
    Process castanets_process;
    // Positive: normal process
    // 0: kNullProcessHandle
    // Negative: Castanets Process
    castanets_process.process =
        base::Process(base::kCastanetsProcessHandle - child_process_id_);
    *launch_result = LAUNCH_RESULT_SUCCESS;
    return castanets_process;
  }
#endif

  Process process;
  process.process = base::LaunchProcess(*command_line(), options);
  *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                             : LAUNCH_RESULT_FAILURE;
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  if (process.zygote) {
    info.status = process.zygote->GetTerminationStatus(
        process.process.Handle(), known_dead, &info.exit_code);
  } else if (known_dead) {
    info.status = base::GetKnownDeadTerminationStatus(process.process.Handle(),
                                                      &info.exit_code);
  } else {
    info.status =
        base::GetTerminationStatus(process.process.Handle(), &info.exit_code);
  }
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  // TODO(https://crbug.com/818244): Determine whether we should also call
  // EnsureProcessTerminated() to make sure of process-exit, and reap it.
  return process.Terminate(exit_code, false);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  process.process.Terminate(service_manager::RESULT_CODE_NORMAL_EXIT, false);

  // On POSIX, we must additionally reap the child.
  if (process.zygote) {
    // If the renderer was created via a zygote, we have to proxy the reaping
    // through the zygote process.
    process.zygote->EnsureProcessTerminated(process.process.Handle());
  } else {
    base::EnsureProcessTerminated(std::move(process.process));
  }
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    const ChildProcessLauncherPriority& priority) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  if (process.CanBackgroundProcesses())
    process.SetProcessBackgrounded(priority.is_background());
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    std::map<std::string, base::FilePath> required_files) {
  SetFilesToShareForServicePosix(service_name, std::move(required_files));
}

// static
void ChildProcessLauncherHelper::ResetRegisteredFilesForTesting() {
  ResetFilesToShareForTestingPosix();
}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  base::FilePath exe_dir;
  bool result = base::PathService::Get(base::BasePathKey::DIR_EXE, &exe_dir);
  DCHECK(result);
  base::File file(exe_dir.Append(path),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  *region = base::MemoryMappedFile::Region::kWholeFile;
  return file;
}

}  // namespace internal
}  // namespace content
