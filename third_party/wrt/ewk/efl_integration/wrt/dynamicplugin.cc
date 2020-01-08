// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrt/dynamicplugin.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "third_party/wrt/common/string_utils.h"
#include "third_party/wrt/xwalk_extensions/renderer/runtime_ipc_client.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_renderer_controller.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_module_system.h"
#include "wrt/wrt_dynamicplugin.h"

DynamicPlugin::DynamicPlugin()
    : version_(0) {}

bool DynamicPlugin::Init() {
  return true;
}

bool DynamicPlugin::InitRenderer() {
    if (!Init())
      return false;

  version_ = DynamicPluginVersion();
  if (version_ != 0 && version_ != 1) {
    LOG(ERROR) << "Unknown plugin version: " << version_ << "!\n";
    return false;
  }

  return true;
}

void DynamicPlugin::StartSession(const char* session_id,
                                 v8::Handle<v8::Context> context,
                                 int routing_handle,
                                 const char* session_blob,
                                 double scale_factor,
                                 const char* encoded_bundle,
                                 const char* theme) const {
  DynamicPluginStartSession(session_id, context, routing_handle, session_blob);
}

void DynamicPlugin::StopSession(const char* session_id,
                                v8::Handle<v8::Context> context) const {
  DynamicPluginStopSession(session_id, context);
}

void DynamicPlugin::DynamicPluginStartSession(const char* tizen_id,
                               v8::Handle<v8::Context> context,
                               int routing_handle,
                               const char* base_url) const {
  // SCOPE_PROFILE();

  // Initialize context's aligned pointer in embedder data with null
  extensions::XWalkModuleSystem::SetModuleSystemInContext(
      std::unique_ptr<extensions::XWalkModuleSystem>(), context);

  if (!base_url || (common::utils::StartsWith(base_url, "http")
#if defined(OS_TIZEN_TV_PRODUCT)
      && !common::privilege::CheckHostedAppPrivilege(
          wrt::ApplicationData::GetInstance().GetPackageID())
#endif
    )) {
    LOG(ERROR) << "External url not allowed plugin loading.";
    return;
  }

  // Initialize RuntimeIPCClient
  extensions::RuntimeIPCClient* rc =
      extensions::RuntimeIPCClient::GetInstance();
  rc->SetRoutingId(context, routing_handle);

  extensions::XWalkExtensionRendererController& controller =
      extensions::XWalkExtensionRendererController::GetInstance();
  controller.DidCreateScriptContext(context);
}

void DynamicPlugin::DynamicPluginStopSession(
    const char* tizen_id, v8::Handle<v8::Context> context) const {
  // SCOPE_PROFILE();
  extensions::XWalkExtensionRendererController& controller =
      extensions::XWalkExtensionRendererController::GetInstance();
  controller.WillReleaseScriptContext(context);
}

DynamicPlugin::~DynamicPlugin() {}

// static
DynamicPlugin& DynamicPlugin::Get(V8Widget::Type type) {
#if defined(OS_TIZEN_TV_PRODUCT)
  if (type == V8Widget::Type::HBBTV)
    return HbbtvDynamicPlugin::Get();
#endif
  DCHECK_EQ(type, V8Widget::Type::WRT);
  return WrtDynamicPlugin::Get();
}

