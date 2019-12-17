// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_adapter.h"

#include <string>

#include "base/logging.h"

namespace extensions {

XWalkExtensionAdapter::XWalkExtensionAdapter()
  : next_xw_extension_(1),
    next_xw_instance_(1) {
}

XWalkExtensionAdapter::~XWalkExtensionAdapter() {
}

XWalkExtensionAdapter* XWalkExtensionAdapter::GetInstance() {
  static XWalkExtensionAdapter self;
  return &self;
}

XW_Extension XWalkExtensionAdapter::GetNextXWExtension() {
  return next_xw_extension_++;
}

XW_Instance XWalkExtensionAdapter::GetNextXWInstance() {
  return next_xw_instance_++;
}

void XWalkExtensionAdapter::RegisterExtension(XWalkExtension* extension) {
  XW_Extension xw_extension = extension->xw_extension_;
  if (!(xw_extension > 0 && xw_extension < next_xw_extension_)) {
    LOG(WARNING) << "xw_extension (" << xw_extension << ") is invalid.";
    return;
  }
  if (extension_map_.find(xw_extension) == extension_map_.end())
    extension_map_[xw_extension] = extension;
}

void XWalkExtensionAdapter::UnregisterExtension(XWalkExtension* extension) {
  XW_Extension xw_extension = extension->xw_extension_;
  if (!(xw_extension > 0 && xw_extension < next_xw_extension_)) {
    LOG(WARNING) << "xw_extension (" << xw_extension << ") is invalid.";
    return;
  }
  auto it = extension_map_.find(xw_extension);
  if (it != extension_map_.end()) {
    extension_map_.erase(it);
  }
}

void XWalkExtensionAdapter::RegisterInstance(
    XWalkExtensionInstance* instance) {
  /*
  XW_Instance xw_instance = instance->xw_instance_;
  if (!(xw_instance > 0 && xw_instance < next_xw_instance_)) {
    LOG(WARNING) << "xw_instance (" << xw_instance << ") is invalid.";
    return;
  }
  if (instance_map_.find(xw_instance) == instance_map_.end())
    instance_map_[xw_instance] = instance;
  */
}

void XWalkExtensionAdapter::UnregisterInstance(
    XWalkExtensionInstance* instance) {
  /*
  XW_Instance xw_instance = instance->xw_instance_;
  if (!(xw_instance > 0 && xw_instance < next_xw_instance_)) {
    LOG(WARNING) << "xw_instance (" << xw_instance << ") is invalid.";
    return;
  }
  auto it = instance_map_.find(xw_instance);
  if (it != instance_map_.end()) {
    instance_map_.erase(it);
  }
  */
}

const void* XWalkExtensionAdapter::GetInterface(const char* name) {
  if (!strcmp(name, XW_CORE_INTERFACE_1)) {
    static const XW_CoreInterface_1 coreInterface1 = {
      CoreSetExtensionName,
      CoreSetJavaScriptAPI,
      CoreRegisterInstanceCallbacks,
      CoreRegisterShutdownCallback,
      CoreSetInstanceData,
      CoreGetInstanceData
    };
    return &coreInterface1;
  }

  if (!strcmp(name, XW_MESSAGING_INTERFACE_1)) {
    static const XW_MessagingInterface_1 messagingInterface1 = {
      MessagingRegister,
      MessagingPostMessage
    };
    return &messagingInterface1;
  }

  if (!strcmp(name, XW_MESSAGING_INTERFACE_2)) {
    static const XW_MessagingInterface_2 messagingInterface2 = {
      MessagingRegister,
      MessagingPostMessage,
      MessagingRegisterBinaryMessageCallback,
      MessagingPostBinaryMessage
    };
    return &messagingInterface2;
  }

  if (!strcmp(name, XW_INTERNAL_SYNC_MESSAGING_INTERFACE_1)) {
    static const XW_Internal_SyncMessagingInterface_1
        syncMessagingInterface1 = {
      SyncMessagingRegister,
      SyncMessagingSetSyncReply
    };
    return &syncMessagingInterface1;
  }

  if (!strcmp(name, XW_INTERNAL_ENTRY_POINTS_INTERFACE_1)) {
    static const XW_Internal_EntryPointsInterface_1 entryPointsInterface1 = {
      EntryPointsSetExtraJSEntryPoints
    };
    return &entryPointsInterface1;
  }

  if (!strcmp(name, XW_INTERNAL_RUNTIME_INTERFACE_1)) {
    static const XW_Internal_RuntimeInterface_1 runtimeInterface1 = {
      RuntimeGetStringVariable
    };
    return &runtimeInterface1;
  }

  if (!strcmp(name, XW_INTERNAL_PERMISSIONS_INTERFACE_1)) {
    static const XW_Internal_PermissionsInterface_1 permissionsInterface1 = {
      PermissionsCheckAPIAccessControl,
      PermissionsRegisterPermissions
    };
    return &permissionsInterface1;
  }

  LOG(WARNING) << "Interface '" << name << "' is not supported.";
  return NULL;
}

XWalkExtension* XWalkExtensionAdapter::GetExtension(XW_Extension xw_extension) {
  XWalkExtensionAdapter* adapter = XWalkExtensionAdapter::GetInstance();
  ExtensionMap::iterator it = adapter->extension_map_.find(xw_extension);
  if (it == adapter->extension_map_.end())
    return NULL;
  return it->second;
}

XWalkExtensionInstance* XWalkExtensionAdapter::GetExtensionInstance(
    XW_Instance xw_instance) {
  XWalkExtensionAdapter* adapter = XWalkExtensionAdapter::GetInstance();
  InstanceMap::iterator it = adapter->instance_map_.find(xw_instance);
  if (it == adapter->instance_map_.end())
    return NULL;
  return it->second;
}

#define CHECK(x, xw)                                                 \
  if (!x) {                                                          \
    LOG(WARNING) << "Ignoring call. Invalid " << #xw << " = " << xw; \
    return;                                                          \
  }

#define RETURN_IF_INITIALIZED(x) \
  if (x->initialized_) \
    return;

void XWalkExtensionAdapter::CoreSetExtensionName(
    XW_Extension xw_extension,
    const char* name) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->name_ = name;
}

void XWalkExtensionAdapter::CoreSetJavaScriptAPI(
    XW_Extension xw_extension,
    const char* javascript_api) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->javascript_api_ = javascript_api;
}

void XWalkExtensionAdapter::CoreRegisterInstanceCallbacks(
    XW_Extension xw_extension,
    XW_CreatedInstanceCallback created,
    XW_DestroyedInstanceCallback destroyed) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->created_instance_callback_ = created;
  extension->destroyed_instance_callback_ = destroyed;
}

void XWalkExtensionAdapter::CoreRegisterShutdownCallback(
    XW_Extension xw_extension,
    XW_ShutdownCallback shutdown) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->shutdown_callback_ = shutdown;
}

void XWalkExtensionAdapter::CoreSetInstanceData(
    XW_Instance xw_instance,
    void* data) {
  XWalkExtensionInstance* instance = GetExtensionInstance(xw_instance);
  CHECK(instance, xw_instance);
  instance->instance_data_ = data;
}

void* XWalkExtensionAdapter::CoreGetInstanceData(
    XW_Instance xw_instance) {
  XWalkExtensionInstance* instance = GetExtensionInstance(xw_instance);
  if (instance)
    return instance->instance_data_;
  else
    return NULL;
}

void XWalkExtensionAdapter::MessagingRegister(
    XW_Extension xw_extension,
    XW_HandleMessageCallback handle_message) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->handle_msg_callback_ = handle_message;
}

void XWalkExtensionAdapter::MessagingPostMessage(
    XW_Instance xw_instance,
    const char* message) {
  XWalkExtensionInstance* instance = GetExtensionInstance(xw_instance);
  CHECK(instance, xw_instance);
  instance->PostMessageToJS(message);
}

void XWalkExtensionAdapter::SyncMessagingRegister(
    XW_Extension xw_extension,
    XW_HandleSyncMessageCallback handle_sync_message) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->handle_sync_msg_callback_ = handle_sync_message;
}

void XWalkExtensionAdapter::SyncMessagingSetSyncReply(
    XW_Instance xw_instance,
    const char* reply) {
  XWalkExtensionInstance* instance = GetExtensionInstance(xw_instance);
  CHECK(instance, xw_instance);
  instance->SyncReplyToJS(reply);
}

void XWalkExtensionAdapter::EntryPointsSetExtraJSEntryPoints(
    XW_Extension xw_extension,
    const char** entry_points) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);

  for (int i=0; entry_points[i]; ++i) {
    extension->entry_points_.push_back(std::string(entry_points[i]));
  }
}

void XWalkExtensionAdapter::RuntimeGetStringVariable(
    XW_Extension xw_extension,
    const char* key,
    char* value,
    unsigned int value_len) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  extension->GetRuntimeVariable(key, value, value_len);
}

int XWalkExtensionAdapter::PermissionsCheckAPIAccessControl(
    XW_Extension xw_extension,
    const char* api_name) {
  XWalkExtension* extension = GetExtension(xw_extension);
  if (extension)
    return extension->CheckAPIAccessControl(api_name);
  else
    return XW_ERROR;
}

int XWalkExtensionAdapter::PermissionsRegisterPermissions(
    XW_Extension xw_extension,
    const char* perm_table) {
  XWalkExtension* extension = GetExtension(xw_extension);
  if (extension)
    return extension->RegisterPermissions(perm_table);
  else
    return XW_ERROR;
}

void XWalkExtensionAdapter::MessagingRegisterBinaryMessageCallback(
  XW_Extension xw_extension, XW_HandleBinaryMessageCallback handle_message) {
  XWalkExtension* extension = GetExtension(xw_extension);
  CHECK(extension, xw_extension);
  RETURN_IF_INITIALIZED(extension);
  extension->handle_binary_msg_callback_ = handle_message;
}

void XWalkExtensionAdapter::MessagingPostBinaryMessage(
  XW_Instance xw_instance, const char* message, size_t size) {
  XWalkExtensionInstance* instance = GetExtensionInstance(xw_instance);
  CHECK(instance, xw_instance);
  instance->PostMessageToJS(message);
}

#undef CHECK
#undef RETURN_IF_INITIALIZED

}  // namespace extensions
