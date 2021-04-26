// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_sandbox_hook.h"

#include <dlfcn.h>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chromeos/services/tts/constants.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace chromeos {
namespace tts {

void AddBundleFolder(std::vector<BrokerFilePermission>* permissions) {
  base::FilePath bundle_dir =
      base::FilePath(FILE_PATH_LITERAL(kLibchromettsPath))
          .AsEndingWithSeparator();
  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive(bundle_dir.value()));
}

void AddTempDataDirectory(std::vector<BrokerFilePermission>* permissions) {
  // TODO: figure out read-write directory for tts.
  base::FilePath rw_dir = base::FilePath(FILE_PATH_LITERAL(kTempDataDirectory))
                              .AsEndingWithSeparator();
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive(rw_dir.value()));
}

std::vector<BrokerFilePermission> GetTtsFilePermissions() {
  std::vector<BrokerFilePermission> permissions;
  AddBundleFolder(&permissions);
  AddTempDataDirectory(&permissions);
  return permissions;
}

bool TtsPreSandboxHook(service_manager::SandboxLinux::Options options) {
  if (!dlopen(kLibchromettsPath, RTLD_LAZY))
    LOG(ERROR) << "Unable to open libchrometts.so: " << dlerror();

  // Ensure this directory is created.
  base::FilePath temp_data_dir(kTempDataDirectory);
  base::CreateDirectoryAndGetError(temp_data_dir, nullptr);
  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_MKDIR,
                                   sandbox::syscall_broker::COMMAND_STAT,
                                   sandbox::syscall_broker::COMMAND_STAT64,
                                   sandbox::syscall_broker::COMMAND_RENAME,
                                   sandbox::syscall_broker::COMMAND_UNLINK,
                               }),
                               GetTtsFilePermissions(),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace tts
}  // namespace chromeos
