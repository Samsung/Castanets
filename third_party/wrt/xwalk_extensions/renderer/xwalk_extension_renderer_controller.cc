// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_renderer_controller.h"

// #include <Ecore.h>
#include <string>
#include <utility>

#include "base/logging.h"
// #include "wrt/common/profiler.h"
#include "third_party/wrt/xwalk_extensions/renderer/object_tools_module.h"
#include "third_party/wrt/xwalk_extensions/renderer/runtime_ipc_client.h"
#include "third_party/wrt/xwalk_extensions/renderer/widget_module.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_client.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_module.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_module_system.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_v8tools_module.h"

namespace extensions {

// static
int XWalkExtensionRendererController::plugin_session_count = 0;

namespace {

void CreateExtensionModules(XWalkExtensionClient* client,
                            XWalkModuleSystem* module_system) {
  const XWalkExtensionClient::ExtensionAPIMap& extensions =
      client->extension_apis();

  for (auto it = extensions.begin(); it != extensions.end(); ++it) {
    XWalkExtensionClient::ExtensionCodePoints* codepoint = it->second;
    std::unique_ptr<XWalkExtensionModule> module(
        new XWalkExtensionModule(client, module_system,
                                 it->first, codepoint->api));
    module_system->RegisterExtensionModule(std::move(module),
                                           codepoint->entry_points);
  }
}

}  // namespace

XWalkExtensionRendererController&
XWalkExtensionRendererController::GetInstance() {
  static XWalkExtensionRendererController* instance = new XWalkExtensionRendererController;
  return *instance;
}

XWalkExtensionRendererController::XWalkExtensionRendererController()
    : exit_requested(false),
      extensions_client_(new XWalkExtensionClient()) {
}

XWalkExtensionRendererController::~XWalkExtensionRendererController() {
}

void XWalkExtensionRendererController::DidCreateScriptContext(
    v8::Handle<v8::Context> context) {
  // SCOPE_PROFILE();

  // Skip plugin loading after application exit request.
  if (exit_requested) {
    return;
  }

  XWalkModuleSystem* module_system = new XWalkModuleSystem(context);
  XWalkModuleSystem::SetModuleSystemInContext(
      std::unique_ptr<XWalkModuleSystem>(module_system), context);
  module_system->RegisterNativeModule(
        "v8tools",
        std::unique_ptr<XWalkNativeModule>(new XWalkV8ToolsModule));
  module_system->RegisterNativeModule(
        "WidgetModule",
        std::unique_ptr<XWalkNativeModule>(new WidgetModule));
  module_system->RegisterNativeModule(
        "objecttools",
        std::unique_ptr<XWalkNativeModule>(new ObjectToolsModule));

  extensions_client_->Initialize(context);
  CreateExtensionModules(extensions_client_.get(), module_system);

  module_system->Initialize();
  plugin_session_count++;
  LOG(INFO) << "plugin_session_count : " << plugin_session_count;
}

void XWalkExtensionRendererController::WillReleaseScriptContext(
    v8::Handle<v8::Context> context) {
  v8::Context::Scope contextScope(context);
  XWalkModuleSystem* module_system = XWalkModuleSystem::GetModuleSystemFromContext(context);
  if (module_system) {
    plugin_session_count--;
    LOG(INFO) << "plugin_session_count : " << plugin_session_count;
  }
  XWalkModuleSystem::ResetModuleSystemFromContext(context);
}

void XWalkExtensionRendererController::OnReceivedIPCMessage(
    const Ewk_IPC_Wrt_Message_Data* data) {

  const char* type = ewk_ipc_wrt_message_data_type_get(data);

#define TYPE_BEGIN(x) (!strncmp(type, x, strlen(x)))
  if (TYPE_BEGIN("xwalk://"))  {
    const char* id = ewk_ipc_wrt_message_data_id_get(data);
    const char* msg = ewk_ipc_wrt_message_data_value_get(data);
    extensions_client_->OnReceivedIPCMessage(id, msg);
  } else {
    RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
    ipc->HandleMessageFromRuntime(data);
  }
#undef TYPE_BEGIN
}

void XWalkExtensionRendererController::InitializeExtensionClient() {
  // extensions_client_->Initialize();
}

void XWalkExtensionRendererController::LoadUserExtensions(
  const std::string app_path) {
  extensions_client_->LoadUserExtensions(app_path);
}

}  // namespace extensions
