// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/vm/vm_permission_service_provider.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/guid.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/vm_permission_service/vm_permission_service.pb.h"
#include "components/prefs/pref_service.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

VmPermissionServiceProvider::VmInfo::VmInfo(std::string owner_id,
                                            std::string name,
                                            VmType type)
    : owner_id_(std::move(owner_id)), name_(std::move(name)), type_(type) {}

VmPermissionServiceProvider::VmInfo::~VmInfo() = default;

VmPermissionServiceProvider::VmPermissionServiceProvider() {}

VmPermissionServiceProvider::~VmPermissionServiceProvider() = default;

VmPermissionServiceProvider::VmMap::iterator
VmPermissionServiceProvider::FindVm(const std::string& owner_id,
                                    const std::string& name) {
  return std::find_if(vms_.begin(), vms_.end(), [&](const auto& vm) {
    return vm.second->owner_id_ == owner_id && vm.second->name_ == name;
  });
}

void VmPermissionServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kVmPermissionServiceInterface, kVmPermissionServiceRegisterVmMethod,
      base::BindRepeating(&VmPermissionServiceProvider::RegisterVm,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kVmPermissionServiceInterface, kVmPermissionServiceUnregisterVmMethod,
      base::BindRepeating(&VmPermissionServiceProvider::UnregisterVm,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kVmPermissionServiceInterface, kVmPermissionServiceGetPermissionsMethod,
      base::BindRepeating(&VmPermissionServiceProvider::GetPermissions,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kVmPermissionServiceInterface, kVmPermissionServiceSetPermissionsMethod,
      base::BindRepeating(&VmPermissionServiceProvider::SetPermissions,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmPermissionServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmPermissionServiceProvider::OnExported(const std::string& interface_name,
                                             const std::string& method_name,
                                             bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void VmPermissionServiceProvider::RegisterVm(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::RegisterVmRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse RegisterVmRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  if (FindVm(request.owner_id(), request.name()) != vms_.end()) {
    LOG(ERROR) << "VM " << request.owner_id() << "/" << request.name()
               << " is already registered with permission service";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "VM is already registered"));
    return;
  }

  VmInfo::VmType vm_type;
  if (request.type() == vm_permission_service::RegisterVmRequest::PLUGIN_VM) {
    vm_type = VmInfo::VmType::PluginVm;
  } else {
    LOG(ERROR) << "Unsupported VM " << request.owner_id() << "/"
               << request.name() << " type: " << request.type();
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Unsupported VM type"));
    return;
  }

  auto vm =
      base::WrapUnique(new VmInfo(request.owner_id(), request.name(), vm_type));

  // Seed the initial set of permission. Because in the initial release we
  // only support static permissions, i.e. for changes to take effect we need
  // to re-launch the VM, we do not need to update them after this.
  UpdateVmPermissions(vm.get());

  const std::string token(base::GenerateGUID());
  vms_[token] = std::move(vm);

  vm_permission_service::RegisterVmResponse payload;
  payload.set_token(token);

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::UnregisterVm(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::UnregisterVmRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse RegisterVmRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "VM " << request.owner_id() << "/" << request.name()
               << " is not registered with permission service";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "VM is not registered"));
    return;
  }

  vms_.erase(iter);

  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::SetPermissions(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::SetPermissionsRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse SetPermissionsRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "VM " << request.owner_id() << "/" << request.name()
               << " is not registered with permission service";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "VM is not registered"));
    return;
  }

  base::flat_map<VmInfo::PermissionType, bool> new_permissions(
      iter->second->permissions_);
  for (const auto& p : request.permissions()) {
    VmInfo::PermissionType kind;
    if (p.kind() == vm_permission_service::Permission::CAMERA) {
      kind = VmInfo::PermissionCamera;
    } else if (p.kind() == vm_permission_service::Permission::MICROPHONE) {
      kind = VmInfo::PermissionMicrophone;
    } else {
      constexpr char error_message[] = "Unknown permission type";
      LOG(ERROR) << error_message;
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, DBUS_ERROR_INVALID_ARGS, error_message));
      return;
    }

    new_permissions[kind] = p.allowed();
  }

  // Commit final version of permissions.
  iter->second->permissions_ = std::move(new_permissions);

  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::GetPermissions(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  dbus::MessageReader reader(method_call);
  vm_permission_service::GetPermissionsRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse GetPermissionsRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  auto iter = vms_.find(request.token());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Invalid token " << request.token();
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "Invalid token"));
    return;
  }

  vm_permission_service::GetPermissionsResponse payload;
  for (auto permission : iter->second->permissions_) {
    auto* p = payload.add_permissions();
    switch (permission.first) {
      case VmInfo::PermissionCamera:
        p->set_kind(vm_permission_service::Permission::CAMERA);
        break;
      case VmInfo::PermissionMicrophone:
        p->set_kind(vm_permission_service::Permission::MICROPHONE);
        break;
    }
    p->set_allowed(permission.second);
  }

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void VmPermissionServiceProvider::UpdateVmPermissions(VmInfo* vm) {
  vm->permissions_.clear();
  switch (vm->type_) {
    case VmInfo::PluginVm:
      UpdatePluginVmPermissions(vm);
      break;
    case VmInfo::CrostiniVm:
      NOTREACHED();
  }
}

void VmPermissionServiceProvider::UpdatePluginVmPermissions(VmInfo* vm) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile || chromeos::ProfileHelper::GetUserIdHashFromProfile(profile) !=
                      vm->owner_id_) {
    return;
  }

  const PrefService* prefs = profile->GetPrefs();
  auto* PluginVmManager =
      plugin_vm::PluginVmManagerFactory::GetForProfile(profile);
  if (base::FeatureList::IsEnabled(
          chromeos::features::kPluginVmShowCameraPermissions) &&
      prefs->GetBoolean(prefs::kVideoCaptureAllowed)) {
    vm->permissions_[VmInfo::PermissionCamera] =
        PluginVmManager->GetPermission(plugin_vm::PermissionType::kCamera);
  }

  if (base::FeatureList::IsEnabled(
          chromeos::features::kPluginVmShowMicrophonePermissions) &&
      prefs->GetBoolean(prefs::kAudioCaptureAllowed)) {
    vm->permissions_[VmInfo::PermissionMicrophone] =
        PluginVmManager->GetPermission(plugin_vm::PermissionType::kMicrophone);
  }
}

}  // namespace chromeos
