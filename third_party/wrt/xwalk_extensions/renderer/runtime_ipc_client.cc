/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
// #include <Eina.h>

#include "base/strings/string16.h"
#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "url/gurl.h"
#include "content/public/renderer/render_view.h"
// #include "third_party/wrt/common/profiler.h"
#include "third_party/wrt/common/string_utils.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_messages.h"
#include "third_party/wrt/xwalk_extensions/renderer/runtime_ipc_client.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_extension_renderer_controller.h"

namespace extensions {

namespace {

const int kRoutingIdEmbedderDataIndex = 12;

}  // namespace

RuntimeIPCClient::JSCallback::JSCallback(v8::Isolate* isolate,
                                         v8::Handle<v8::Function> callback) {
  callback_.Reset(isolate, callback);
}

RuntimeIPCClient::JSCallback::~JSCallback() {
  callback_.Reset();
}

void RuntimeIPCClient::JSCallback::Call(v8::Isolate* isolate,
                                        v8::Handle<v8::Value> args[]) {
  if (!callback_.IsEmpty()) {
    v8::HandleScope handle_scope(isolate);
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    if (context.IsEmpty()) {
     // If there's no JavaScript on the stack, we have to make a new Context.
      context = v8::Context::New(isolate);
    }
    v8::Context::Scope scope(context);

    v8::Handle<v8::Function> func =
        v8::Local<v8::Function>::New(isolate, callback_);
    func->Call(func, 1, args);
    if (try_catch.HasCaught()) {
      LOG(ERROR) << "Exception when running Javascript callback";
      v8::String::Utf8Value exception_str(isolate, try_catch.Exception());
      LOG(ERROR) << (*exception_str);
    }
  }
}

// static
RuntimeIPCClient* RuntimeIPCClient::GetInstance() {
  static RuntimeIPCClient self;
  return &self;
}

RuntimeIPCClient::RuntimeIPCClient() {
}

int RuntimeIPCClient::GetRoutingId(v8::Handle<v8::Context> context) {
  v8::Handle<v8::Value> value =
      context->GetEmbedderData(kRoutingIdEmbedderDataIndex);
  int routing_id = 0;
  if (value->IsNumber()) {
    routing_id = value->IntegerValue();
  } else {
    LOG(WARNING) << "Failed to get routing index from context.";
  }

  return routing_id;
}

void RuntimeIPCClient::SetRoutingId(v8::Handle<v8::Context> context,
                                    int routing_id) {
  context->SetEmbedderData(kRoutingIdEmbedderDataIndex,
                           v8::Integer::New(context->GetIsolate(), routing_id));
}

void RuntimeIPCClient::SendMessage(v8::Handle<v8::Context> context,
                                   const std::string& type,
                                   const std::string& value) {
  SendMessage(context, type, "", "", value);
}

void RuntimeIPCClient::SendMessage(v8::Handle<v8::Context> context,
                                   const std::string& type,
                                   const std::string& id,
                                   const std::string& value) {
  SendMessage(context, type, id, "", value);
}

void RuntimeIPCClient::SendMessage(v8::Handle<v8::Context> context,
                                   const std::string& type,
                                   const std::string& id,
                                   const std::string& ref_id,
                                   const std::string& value) {
  if (!strcmp(type.c_str(), "tizen://exit")) {
    extensions::XWalkExtensionRendererController& controller =
      extensions::XWalkExtensionRendererController::GetInstance();
    controller.exit_requested = true;
  }

  int routing_id = GetRoutingId(context);
  if (routing_id < 1) {
    LOG(ERROR) << "Invalid routing handle for IPC.";
    return;
  }

  Ewk_IPC_Wrt_Message_Data* msg = ewk_ipc_wrt_message_data_new();
  ewk_ipc_wrt_message_data_type_set(msg, type.c_str());
  ewk_ipc_wrt_message_data_id_set(msg, id.c_str());
  ewk_ipc_wrt_message_data_reference_id_set(msg, ref_id.c_str());
  ewk_ipc_wrt_message_data_value_set(msg, value.c_str());
  content::RenderView* render_view =
      content::RenderView::FromRoutingID(routing_id);
  render_view->Send(
      new XWalkExtensionHostMsg_Message(render_view->GetRoutingID(), *msg));
  ewk_ipc_wrt_message_data_del(msg);

}

std::string RuntimeIPCClient::SendSyncMessage(v8::Handle<v8::Context> context,
                                              const std::string& type,
                                              const std::string& value) {
  return SendSyncMessage(context, type, "", "", value);
}

std::string RuntimeIPCClient::SendSyncMessage(v8::Handle<v8::Context> context,
                                              const std::string& type,
                                              const std::string& id,
                                              const std::string& value) {
  return SendSyncMessage(context, type, id, "", value);
}

std::string RuntimeIPCClient::SendSyncMessage(v8::Handle<v8::Context> context,
                                              const std::string& type,
                                              const std::string& id,
                                              const std::string& ref_id,
                                              const std::string& value) {
  int routing_id = GetRoutingId(context);
  if (routing_id < 1) {
    LOG(ERROR) << "Invalid routing handle for IPC.";
    return std::string();
  }

  Ewk_IPC_Wrt_Message_Data* msg = ewk_ipc_wrt_message_data_new();
  ewk_ipc_wrt_message_data_type_set(msg, type.c_str());
  ewk_ipc_wrt_message_data_id_set(msg, id.c_str());
  ewk_ipc_wrt_message_data_reference_id_set(msg, ref_id.c_str());
  ewk_ipc_wrt_message_data_value_set(msg, value.c_str());

  content::RenderView* render_view =
      content::RenderView::FromRoutingID(routing_id);
  render_view->Send(
      new XWalkExtensionHostMsg_Message_Sync(render_view->GetRoutingID(), *msg, &msg->value));

  const char* msg_value = ewk_ipc_wrt_message_data_value_get(msg);
  if (!msg_value)
    return std::string();
  std::string result(msg_value);

  ewk_ipc_wrt_message_data_del(msg);

  return result;
}

void RuntimeIPCClient::SendAsyncMessage(v8::Handle<v8::Context> context,
                                        const std::string& type,
                                        const std::string& value,
                                        ReplyCallback callback) {
  int routing_id = GetRoutingId(context);
  if (routing_id < 1) {
    LOG(ERROR) << "Invalid routing handle for IPC.";
    return;
  }

  std::string msg_id = common::utils::GenerateUUID();

  Ewk_IPC_Wrt_Message_Data* msg = ewk_ipc_wrt_message_data_new();
  ewk_ipc_wrt_message_data_id_set(msg, msg_id.c_str());
  ewk_ipc_wrt_message_data_type_set(msg, type.c_str());
  ewk_ipc_wrt_message_data_value_set(msg, value.c_str());

  content::RenderView* render_view =
      content::RenderView::FromRoutingID(routing_id);
  render_view->Send(
      new XWalkExtensionHostMsg_Message(render_view->GetRoutingID(), *msg));

  callbacks_[msg_id] = callback;

  ewk_ipc_wrt_message_data_del(msg);
}

void RuntimeIPCClient::HandleMessageFromRuntime(
    const Ewk_IPC_Wrt_Message_Data* msg) {
  if (msg == NULL) {
    LOG(ERROR) << "received message is NULL";
    return;
  }

  const char* msg_refid = ewk_ipc_wrt_message_data_reference_id_get(msg);

  if (msg_refid == NULL || !strcmp(msg_refid, "")) {
    LOG(ERROR) << "No reference id of received message.";
    return;
  }

  auto it = callbacks_.find(msg_refid);
  if (it == callbacks_.end()) {
    LOG(ERROR) << "No registered callback with reference id : " << msg_refid;
    return;
  }

  const char* msg_type = ewk_ipc_wrt_message_data_type_get(msg);
  const char* msg_value = ewk_ipc_wrt_message_data_value_get(msg);

  ReplyCallback func = it->second;
  if (func) {
    func(msg_type, msg_value);
  }

  callbacks_.erase(it);
}

}  // namespace extensions
