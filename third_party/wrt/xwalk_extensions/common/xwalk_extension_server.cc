// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_server.h"

// #include <Ecore.h>
#include <string>

#include "content/public/browser/render_process_host.h"
// #include "public/EWebKit_internal.h"
// #include "tizen_src/ewk/efl_integration/wrt/wrt_widget_host.h"
// #include "third_party/wrt/common/profiler.h"
#include "third_party/wrt/common/string_utils.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_constants.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_manager.h"

namespace extensions {

// static
XWalkExtensionServer* XWalkExtensionServer::GetInstance() {
  static XWalkExtensionServer self;
  return &self;
}

XWalkExtensionServer::XWalkExtensionServer() {
  manager_.LoadExtensions();
}

XWalkExtensionServer::~XWalkExtensionServer() {
}

void XWalkExtensionServer::Preload() {
  manager_.PreloadExtensions();
}

void XWalkExtensionServer::Shutdown() {
  base::AutoLock locker(extension_server_lock_);
  for (auto it = instances_.begin(); it != instances_.end(); ++it) {
    delete it->second;
  }
  instances_.clear();
  manager_.UnloadExtensions();
}
/*
Json::Value XWalkExtensionServer::GetExtensions() {
  base::AutoLock locker(extension_server_lock_);
  Json::Value out;
  auto extensions = manager_.extensions();
  for (auto it = extensions.begin(); it != extensions.end(); ++it) {
    Json::Value ext;
    ext["name"] = it->second->name();
    // ext["api"] = it->second->GetJavascriptCode();
    auto entry_points = it->second->entry_points();
    for (auto ite = entry_points.begin(); ite != entry_points.end(); ++ite) {
      ext["entry_points"].append(*ite);
    }
    out.append(ext);
  }
  return out;
}

std::string XWalkExtensionServer::GetAPIScript(
    const std::string& extension_name) {
  base::AutoLock locker(extension_server_lock_);
  auto extensions = manager_.extensions();
  auto it = extensions.find(extension_name);
  if (it == extensions.end()) {
    LOG(ERROR) << "No such extension '" << extension_name << "'";
    return std::string();
  }

  return it->second->GetJavascriptCode();
}

std::string XWalkExtensionServer::CreateInstance(
    const std::string& extension_name) {
  base::AutoLock locker(extension_server_lock_);
  std::string instance_id;
  auto extensions = manager_.extensions();
  auto it = extensions.find(extension_name);
  if (it != extensions.end()) {
    XWalkExtensionInstance* instance = it->second->CreateInstance();
    if (instance) {
      instance_id = common::utils::GenerateUUID();
      instance->SetPostMessageCallback(
          [this, instance_id](const std::string& msg) {
        Ewk_IPC_Wrt_Message_Data* ans = ewk_ipc_wrt_message_data_new();
        ewk_ipc_wrt_message_data_type_set(ans, kMethodPostMessageToJS);
        ewk_ipc_wrt_message_data_id_set(ans, instance_id.c_str());
        ewk_ipc_wrt_message_data_value_set(ans, msg.c_str());
        WrtWidgetHost::Get()->SendWrtMessage(*ans);
        ewk_ipc_wrt_message_data_del(ans);
      });

      instances_[instance_id] = instance;
    } else {
      LOG(ERROR) << "Failed to create instance of the extension '"
                 << extension_name << "'";
    }
  } else {
    LOG(ERROR) << "No such extension '" << extension_name << "'";
  }
  LOG(ERROR) << instance_id;
  return instance_id;
}

void XWalkExtensionServer::HandleIPCMessage(Ewk_IPC_Wrt_Message_Data* data) {
  if (!data) {
    LOG(ERROR) << "Invalid parameter. data is NULL.";
    return;
  }

  Eina_Stringshare* msg_type = ewk_ipc_wrt_message_data_type_get(data);
  #define TYPE_IS(x) (!strcmp(msg_type, x))

  if (TYPE_IS(kMethodGetExtensions)) {
    HandleGetExtensions(data);
  } else if (TYPE_IS(kMethodCreateInstance)) {
    HandleCreateInstance(data);
  } else if (TYPE_IS(kMethodDestroyInstance)) {
    HandleDestroyInstance(data);
  } else if (TYPE_IS(kMethodPostMessage)) {
    HandlePostMessageToNative(data);
  } else if (TYPE_IS(kMethodSendSyncMessage)) {
    HandleSendSyncMessageToNative(data);
  } else if (TYPE_IS(kMethodGetAPIScript)) {
    HandleGetAPIScript(data);
  }

  eina_stringshare_del(msg_type);
  #undef TYPE_IS
}

void XWalkExtensionServer::HandleGetExtensions(Ewk_IPC_Wrt_Message_Data* data) {
  Json::Value reply = GetExtensions();
  Json::FastWriter writer;
  std::string reply_str = writer.write(reply);
  ewk_ipc_wrt_message_data_value_set(data, reply_str.c_str());
}

void XWalkExtensionServer::HandleCreateInstance(
    Ewk_IPC_Wrt_Message_Data* data) {
  Eina_Stringshare* extension_name = ewk_ipc_wrt_message_data_value_get(data);

  std::string instance_id = CreateInstance(extension_name);

  ewk_ipc_wrt_message_data_value_set(data, instance_id.c_str());

  eina_stringshare_del(extension_name);
}

void XWalkExtensionServer::HandleDestroyInstance(
    Ewk_IPC_Wrt_Message_Data* data) {
  Eina_Stringshare* instance_id = ewk_ipc_wrt_message_data_id_get(data);

  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    XWalkExtensionInstance* instance = it->second;
    delete instance;
    instances_.erase(it);
  } else {
    LOG(ERROR) << "No such instance '" << instance_id << "'";
  }

  eina_stringshare_del(instance_id);
}

void XWalkExtensionServer::HandlePostMessageToNative(
    Ewk_IPC_Wrt_Message_Data* data) {
  Eina_Stringshare* instance_id = ewk_ipc_wrt_message_data_id_get(data);

  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    Eina_Stringshare* msg = ewk_ipc_wrt_message_data_value_get(data);
    XWalkExtensionInstance* instance = it->second;
    instance->HandleMessage(msg);
    eina_stringshare_del(msg);
  } else {
    LOG(ERROR) << "No such instance '" << instance_id << "'";
  }

  eina_stringshare_del(instance_id);
}

void XWalkExtensionServer::HandleSendSyncMessageToNative(
    Ewk_IPC_Wrt_Message_Data* data) {
  Eina_Stringshare* instance_id = ewk_ipc_wrt_message_data_id_get(data);

  auto it = instances_.find(instance_id);
  if (it != instances_.end()) {
    Eina_Stringshare* msg = ewk_ipc_wrt_message_data_value_get(data);
    XWalkExtensionInstance* instance = it->second;
    std::string reply;
    instance->SetSendSyncReplyCallback([&reply](const std::string& msg) {
      reply = msg;
    });
    instance->HandleSyncMessage(msg);
    ewk_ipc_wrt_message_data_value_set(data, reply.c_str());
    eina_stringshare_del(msg);
  } else {
    LOG(ERROR) << "No such instance '" << instance_id << "'";
  }

  eina_stringshare_del(instance_id);
}

void XWalkExtensionServer::HandleGetAPIScript(
    Ewk_IPC_Wrt_Message_Data* data) {
  Eina_Stringshare* extension_name = ewk_ipc_wrt_message_data_value_get(data);

  std::string api = GetAPIScript(extension_name);

  ewk_ipc_wrt_message_data_value_set(data, api.c_str());

  eina_stringshare_del(extension_name);
}
*/
void XWalkExtensionServer::LoadUserExtensions(const std::string app_path) {
  manager_.LoadUserExtensions(app_path);
}
/*
void XWalkExtensionServer::SendWrtMessage(
    Eina_Stringshare* type, Eina_Stringshare* id, const char* val) {
  Ewk_IPC_Wrt_Message_Data* ans = ewk_ipc_wrt_message_data_new();
  ewk_ipc_wrt_message_data_type_set(ans, type);
  ewk_ipc_wrt_message_data_reference_id_set(ans, id);
  ewk_ipc_wrt_message_data_value_set(ans, val);
  WrtWidgetHost::Get()->SendWrtMessage(*ans);
  ewk_ipc_wrt_message_data_del(ans);
}
*/

}  // namespace extensions
