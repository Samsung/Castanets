// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_client.h"

// #include <Ecore.h>
#include <json/json.h>
#include <unistd.h>

#include <string>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
// #include "third_party/wrt/common/profiler.h"
#include "third_party/wrt/common/string_utils.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_constants.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_server.h"
#include "third_party/wrt/xwalk_extensions/renderer/runtime_ipc_client.h"

namespace extensions {

namespace {
  // we don't have to care single process case.
  /*
  void* CreateInstanceInMainloop(void* data) {
    const char* extension_name = static_cast<const char*>(data);
    XWalkExtensionServer* server = XWalkExtensionServer::GetInstance();
    std::string instance_id = server->CreateInstance(extension_name);
    return static_cast<void*>(new std::string(instance_id));
  }
  */
}  // namespace

XWalkExtensionClient::XWalkExtensionClient() {
}

XWalkExtensionClient::~XWalkExtensionClient() {
  for (auto it = extension_apis_.begin(); it != extension_apis_.end(); ++it) {
    delete it->second;
  }
  extension_apis_.clear();
}

void XWalkExtensionClient::Initialize(v8::Handle<v8::Context> context) {
  // SCOPE_PROFILE();
  if (!extension_apis_.empty()) {
    return;
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  Json::Value reply;
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    // we don't have to care single process case.
    /*
    XWalkExtensionServer* server = XWalkExtensionServer::GetInstance();
    reply = server->GetExtensions();
    */
  } else {
    RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
    std::string extension_info =ipc->SendSyncMessage(context, kMethodGetExtensions, "", "");
    Json::Reader reader;
    reader.parse(extension_info, reply, false);
  }
  for (auto it = reply.begin(); it != reply.end(); ++it) {
    ExtensionCodePoints* codepoint = new ExtensionCodePoints;
    Json::Value entry_points = (*it)["entry_points"];
    for (auto ep = entry_points.begin(); ep != entry_points.end(); ++ep) {
      codepoint->entry_points.push_back((*ep).asString());
    }
    std::string name = (*it)["name"].asString();
    extension_apis_[name] = codepoint;
  }
}

std::string XWalkExtensionClient::CreateInstance(
    v8::Handle<v8::Context> context,
    const std::string& extension_name, InstanceHandler* handler) {
  std::string instance_id;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    // we don't have to care single process case.
    /*
    void* ret = ecore_main_loop_thread_safe_call_sync(
        CreateInstanceInMainloop,
        static_cast<void*>(const_cast<char*>(extension_name.data())));
    std::string* sp = static_cast<std::string*>(ret);
    instance_id = *sp;
    delete sp;
    */
  } else {
    RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
    instance_id =ipc->SendSyncMessage(
        context, kMethodCreateInstance, "", extension_name.data());
  }
  handlers_[instance_id] = handler;
  return instance_id;
}

void XWalkExtensionClient::DestroyInstance(
    v8::Handle<v8::Context> context, const std::string& instance_id) {
  auto it = handlers_.find(instance_id);
  if (it == handlers_.end()) {
    LOG(INFO) << "Failed to destory invalid instance id: " << instance_id;
    return;
  }
  RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
  ipc->SendMessage(context, kMethodDestroyInstance, instance_id, "");

  handlers_.erase(it);
}

void XWalkExtensionClient::PostMessageToNative(
    v8::Handle<v8::Context> context,
    const std::string& instance_id, const std::string& msg) {
  RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
  ipc->SendMessage(context, kMethodPostMessage, instance_id, msg);
}

std::string XWalkExtensionClient::SendSyncMessageToNative(
    v8::Handle<v8::Context> context,
    const std::string& instance_id, const std::string& msg) {
  RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
  std::string reply =
      ipc->SendSyncMessage(context, kMethodSendSyncMessage, instance_id, msg);
  return reply;
}

std::string XWalkExtensionClient::GetAPIScript(
    v8::Handle<v8::Context> context,
    const std::string& extension_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    // we don't have to care single process case.
    /*
    XWalkExtensionServer* server = XWalkExtensionServer::GetInstance();
    return server->GetAPIScript(extension_name);
    */
  } else {
    RuntimeIPCClient* ipc = RuntimeIPCClient::GetInstance();
    std::string reply = ipc->SendSyncMessage(
        context, kMethodGetAPIScript, "", extension_name.data());
    return reply;
  }
}

void XWalkExtensionClient::OnReceivedIPCMessage(
    const std::string& instance_id, const std::string& msg) {
  auto it = handlers_.find(instance_id);
  if (it == handlers_.end()) {
    LOG(INFO) << "Failed to post the message. Invalid instance id.";
    return;
  }

  if (!it->second)
    return;

  it->second->HandleMessageFromNative(msg);
}

void XWalkExtensionClient::LoadUserExtensions(const std::string app_path) {
  // we don't have to care single process case.
  /*
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    XWalkExtensionServer* server = XWalkExtensionServer::GetInstance();
    server->LoadUserExtensions(app_path);
  }
  */
}

}  // namespace extensions
