// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_RENDERER_XWALK_EXTENSION_RENDERER_CONTROLLER_H_
#define XWALK_EXTENSIONS_RENDERER_XWALK_EXTENSION_RENDERER_CONTROLLER_H_

#include <memory>
#include <string>

#include "public/ewk_ipc_message_internal.h"
#include "v8/include/v8.h"

namespace extensions {

class XWalkExtensionClient;

class XWalkExtensionRendererController {
 public:
  static XWalkExtensionRendererController& GetInstance();
  static int plugin_session_count;

  void DidCreateScriptContext(v8::Handle<v8::Context> context);
  void WillReleaseScriptContext(v8::Handle<v8::Context> context);

  void OnReceivedIPCMessage(const Ewk_Wrt_Message_Data* data);

  void InitializeExtensionClient();
  void LoadUserExtensions(const std::string app_path);

  bool exit_requested;

 private:
  XWalkExtensionRendererController();
  virtual ~XWalkExtensionRendererController();

 private:
  std::unique_ptr<XWalkExtensionClient> extensions_client_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_RENDERER_XWALK_EXTENSION_RENDERER_CONTROLLER_H_
