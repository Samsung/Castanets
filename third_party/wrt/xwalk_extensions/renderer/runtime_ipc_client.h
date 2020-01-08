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

#ifndef XWALK_EXTENSIONS_RENDERER_RUNTIME_IPC_CLIENT_H_
#define XWALK_EXTENSIONS_RENDERER_RUNTIME_IPC_CLIENT_H_

#include <functional>
#include <map>
#include <string>

#include "public/ewk_ipc_message_internal.h"
#include "v8/include/v8.h"

namespace extensions {

class RuntimeIPCClient {
 public:
  class JSCallback {
   public:
    explicit JSCallback(v8::Isolate* isolate,
                        v8::Handle<v8::Function> callback);
    ~JSCallback();

    void Call(v8::Isolate* isolate, v8::Handle<v8::Value> args[]);
   private:
    v8::Persistent<v8::Function> callback_;
  };

  typedef std::function<void(const std::string& type,
                             const std::string& value)> ReplyCallback;

  static RuntimeIPCClient* GetInstance();

  // Send message to BrowserProcess without reply
  void SendMessage(v8::Handle<v8::Context> context,
                   const std::string& type,
                   const std::string& value);

  void SendMessage(v8::Handle<v8::Context> context,
                   const std::string& type,
                   const std::string& id,
                   const std::string& value);

  void SendMessage(v8::Handle<v8::Context> context,
                   const std::string& type,
                   const std::string& id,
                   const std::string& ref_id,
                   const std::string& value);

  // Send message to BrowserProcess synchronous with reply
  std::string SendSyncMessage(v8::Handle<v8::Context> context,
                              const std::string& type,
                              const std::string& value);

  std::string SendSyncMessage(v8::Handle<v8::Context> context,
                              const std::string& type,
                              const std::string& id,
                              const std::string& value);

  std::string SendSyncMessage(v8::Handle<v8::Context> context,
                              const std::string& type,
                              const std::string& id,
                              const std::string& ref_id,
                              const std::string& value);

  // Send message to BrowserProcess asynchronous,
  // reply message will be passed to callback function.
  void SendAsyncMessage(v8::Handle<v8::Context> context,
                        const std::string& type, const std::string& value,
                        ReplyCallback callback);

  void HandleMessageFromRuntime(const Ewk_Wrt_Message_Data* msg);

  int GetRoutingId(v8::Handle<v8::Context> context);

  void SetRoutingId(v8::Handle<v8::Context> context, int routing_id);

 private:
  RuntimeIPCClient();

  std::map<std::string, ReplyCallback> callbacks_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_RENDERER_RUNTIME_IPC_CLIENT_H_
