// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrt/wrt_dynamicplugin.h"

#include <dlfcn.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "common/content_switches_efl.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_renderer_controller.h"

WrtDynamicPlugin::WrtDynamicPlugin()
    : DynamicPlugin(),
      widget_info_set_(false) {
}

bool WrtDynamicPlugin::Init() {
  if (!DynamicPlugin::Init())
    return false;
  return true;
}

bool WrtDynamicPlugin::InitRenderer() {
  if (!DynamicPlugin::InitRenderer())
    return false;

  DynamicDatabaseAttach(1);
  return true;
}

void WrtDynamicPlugin::StartSession(const char* tizen_app_id,
                                    v8::Handle<v8::Context> context,
                                    int routing_handle,
                                    const char* base_url,
                                    double scale_factor,
                                    const char* encoded_bundle,
                                    const char* theme) {
  LOG(INFO) << "WrtDynamicPlugin::StartSession";
  DynamicPlugin::StartSession(tizen_app_id, context, routing_handle, base_url,
                              scale_factor, encoded_bundle, theme);
}

void WrtDynamicPlugin::StopSession(const char* tizen_app_id,
                                   v8::Handle<v8::Context> context) {
  DynamicPlugin::StopSession(tizen_app_id, context);
}

bool WrtDynamicPlugin::CanHandleParseUrl(const std::string& scheme) const {
  // xwalk handles only file and app scheme.
  if (scheme == url::kFileScheme || scheme == "app")
    return true;
  return false;
}

void WrtDynamicPlugin::ParseURL(std::string* old_url,
                                std::string* new_url,
                                const char* tizen_app_id,
                                bool* is_encrypted_file) {
  if (!widget_info_set_) {
    // When a web app is launched for first time after reboot, SetWidgetInfo is
    // called later than ParseURL as RenderThread is not yet ready due to webkit
    // initialization taking more time. WRT expects tizen app id to be set
    // before requesting for URL parsing. So we manually set widget info for
    // first time.
    SetWidgetInfo(tizen_app_id);
  }

  DynamicUrlParsing(old_url, new_url, tizen_app_id);
}

void WrtDynamicPlugin::SetWidgetInfo(const std::string& tizen_app_id) {
  if (widget_info_set_) {
    LOG(INFO) << "Widget info is already set!";
    return;
  }

  DynamicSetWidgetInfo(tizen_app_id.c_str());
  widget_info_set_ = true;
}

void WrtDynamicPlugin::DynamicSetWidgetInfo(const char* tizen_id) {
  // SCOPE_PROFILE();
  // ecore_init();

  // runtime::BundleGlobalData::GetInstance()->Initialize(tizen_id);
  extensions::XWalkExtensionRendererController& controller =
    extensions::XWalkExtensionRendererController::GetInstance();
  // auto& app_data = wrt::ApplicationData::GetInstance();
  // controller.LoadUserExtensions(app_data.application_path());
}

void WrtDynamicPlugin::DynamicOnIPCMessage(const Ewk_Wrt_Message_Data& data) {
  // SCOPE_PROFILE();
  extensions::XWalkExtensionRendererController& controller =
    extensions::XWalkExtensionRendererController::GetInstance();
  controller.OnReceivedIPCMessage(&data);
}

void WrtDynamicPlugin::DynamicUrlParsing(
    std::string* old_url, std::string* new_url, const char* tizen_id) {
  LOG(INFO) << "DynamicUrlParsing";
  /*
  auto res_manager =
      runtime::BundleGlobalData::GetInstance()->resource_manager();
  if (res_manager == NULL) {
    LOG(ERROR) << "Widget Info was not set, Resource Manager is NULL";
    *new_url = *old_url;
    return;
  }
  // Check Access control
  if (!res_manager->AllowedResource(*old_url)) {
    // To maintain backward compatibility, we shoudn't explicitly set URL "about:blank"
    *new_url = std::string();
    LOG(ERROR) << "request was blocked by WARP";
    return;
  }
  // convert to localized path
  if (common::utils::StartsWith(*old_url, "file:/") ||
      common::utils::StartsWith(*old_url, "app:/")) {
    *new_url = res_manager->GetLocalizedPath(*old_url);
  } else {
    *new_url = *old_url;
  }
  // check encryption
  if (res_manager->IsEncrypted(*new_url)) {
    *new_url = res_manager->DecryptResource(*new_url);
  }
  */
}

WrtDynamicPlugin::~WrtDynamicPlugin() {
  DynamicDatabaseAttach(0);
}

WrtDynamicPlugin& WrtDynamicPlugin::Get() {
  static WrtDynamicPlugin dynamicPlugin;
  return dynamicPlugin;
}

void WrtDynamicPlugin::MessageReceived(const Ewk_Wrt_Message_Data& data) {
  DynamicOnIPCMessage(data);
}
