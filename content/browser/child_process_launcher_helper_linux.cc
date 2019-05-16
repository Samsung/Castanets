// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_communication_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/zygote_handle_linux.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "gpu/config/gpu_switches.h"

#if defined(CASTANETS)
#include "base/distributed_chromium_util.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "mojo/edk/embedder/tcp_platform_handle_utils.h"
#endif

namespace content {
namespace internal {

mojo::edk::ScopedPlatformHandle
ChildProcessLauncherHelper::PrepareMojoPipeHandlesOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
#if defined(CASTANETS)
  if (base::Castanets::IsEnabled()) {
    mojo_client_handle_ = mojo::edk::ScopedPlatformHandle(
        mojo::edk::PlatformHandle(mojo::edk::kCastanetsHandle));

    LOG(INFO) << " Launching Process: " << GetProcessType();
    if (GetProcessType() == switches::kUtilityProcess)
      return mojo::edk::CreateTCPServerHandle(mojo::edk::kCastanetsUtilitySyncPort);
    else
      return mojo::edk::CreateTCPServerHandle(mojo::edk::kCastanetsSyncPort);
  } else {
    return mojo::edk::ScopedPlatformHandle();
  }
#else
  return mojo::edk::ScopedPlatformHandle();
#endif
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  return CreateDefaultPosixFilesToMap(child_process_id(), mojo_client_handle(),
                                      true /* include_service_required_files */,
                                      GetProcessType(), command_line());
}

void ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    const PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  // Convert FD mapping to FileHandleMappingVector
  options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
      base::GlobalDescriptors::kBaseDescriptor);

  if (GetProcessType() == switches::kRendererProcess ||
      (GetProcessType() == switches::kGpuProcess &&
       base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableOOPRasterization))) {
    const int sandbox_fd = SandboxHostLinux::GetInstance()->GetChildSocket();
    options->fds_to_remap.push_back(std::make_pair(sandbox_fd, GetSandboxFD()));
  }

  #if defined(CASTANETS)
  // Request discovery client to run renderer process on the remote node.
  if (base::Castanets::IsEnabled() &&
          GetProcessType() == switches::kRendererProcess) {
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::SHARED;
    scoped_refptr<dbus::Bus> bus = new dbus::Bus(bus_options);

    dbus::ObjectProxy* object_proxy =
        bus->GetObjectProxy("discovery.client.listener",
                            dbus::ObjectPath("/discovery/client/object"));

    dbus::MethodCall method_call("discovery.client.interface",
                                 "RunService");

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

  options->environ = delegate_->GetEnvironment();
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;

  ZygoteHandle zygote_handle =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoZygote)
          ? nullptr
          : delegate_->GetZygote();
  if (zygote_handle) {
    // TODO(crbug.com/569191): If chrome supported multiple zygotes they could
    // be created lazily here, or in the delegate GetZygote() implementations.
    // Additionally, the delegate could provide a UseGenericZygote() method.
    base::ProcessHandle handle = zygote_handle->ForkRequest(
        command_line()->argv(), std::move(files_to_register), GetProcessType());
    *launch_result = LAUNCH_RESULT_SUCCESS;
    Process process;
    process.process = base::Process(handle);
    process.zygote = zygote_handle;
    return process;
  }
#if defined(CASTANETS)
  if (base::Castanets::IsEnabled()) {
    Process fake_process;
    fake_process.process = base::Process(7777);
    *launch_result = LAUNCH_RESULT_SUCCESS;
    return fake_process;
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

base::TerminationStatus ChildProcessLauncherHelper::GetTerminationStatus(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead,
    int* exit_code) {
  if (process.zygote) {
    return process.zygote->GetTerminationStatus(
        process.process.Handle(), known_dead, exit_code);
  }
  if (known_dead) {
    return base::GetKnownDeadTerminationStatus(
        process.process.Handle(), exit_code);
  }
  return base::GetTerminationStatus(process.process.Handle(), exit_code);
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(
    const base::Process& process, int exit_code, bool wait) {
#if defined(CASTANETS)
  if (base::Castanets::IsEnabled())
    return true;
#endif
  return process.Terminate(exit_code, wait);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
#if defined(CASTANETS)
  if (!base::Castanets::IsEnabled())
    process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
#else
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
#endif
  // On POSIX, we must additionally reap the child.
  if (process.zygote) {
    // If the renderer was created via a zygote, we have to proxy the reaping
    // through the zygote process.
    process.zygote->EnsureProcessTerminated(process.process.Handle());
  } else {
#if defined(CASTANETS)
    if (!base::Castanets::IsEnabled())
      base::EnsureProcessTerminated(std::move(process.process));
#else
    base::EnsureProcessTerminated(std::move(process.process));
#endif
  }
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    const ChildProcessLauncherPriority& priority) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  if (process.CanBackgroundProcesses())
    process.SetProcessBackgrounded(priority.background);
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    catalog::RequiredFileMap required_files) {
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
