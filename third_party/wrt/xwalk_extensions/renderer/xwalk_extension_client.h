// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_RENDERER_XWALK_EXTENSION_CLIENT_H_
#define XWALK_EXTENSIONS_RENDERER_XWALK_EXTENSION_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "v8/include/v8.h"
#include "third_party/wrt/xwalk_extensions/renderer/xwalk_module_system.h"

namespace extensions {

class XWalkExtensionClient {
 public:
  struct InstanceHandler {
    virtual void HandleMessageFromNative(const std::string& msg) = 0;
   protected:
    ~InstanceHandler() {}
  };

  XWalkExtensionClient();
  virtual ~XWalkExtensionClient();

  void Initialize(v8::Handle<v8::Context> context);

  std::string CreateInstance(v8::Handle<v8::Context> context,
                             const std::string& extension_name,
                             InstanceHandler* handler);
  void DestroyInstance(v8::Handle<v8::Context> context,
                       const std::string& instance_id);

  void PostMessageToNative(v8::Handle<v8::Context> context,
                           const std::string& instance_id,
                           const std::string& msg);
  std::string SendSyncMessageToNative(v8::Handle<v8::Context> context,
                                      const std::string& instance_id,
                                      const std::string& msg);

  std::string GetAPIScript(v8::Handle<v8::Context> context,
                           const std::string& extension_name);

  void OnReceivedIPCMessage(const std::string& instance_id,
                            const std::string& msg);
  void LoadUserExtensions(const std::string app_path);

  struct ExtensionCodePoints {
    std::string api;
    std::vector<std::string> entry_points;
  };

  typedef std::map<std::string, ExtensionCodePoints*> ExtensionAPIMap;

  const ExtensionAPIMap& extension_apis() const { return extension_apis_; }

 private:
  ExtensionAPIMap extension_apis_;

  typedef std::map<std::string, InstanceHandler*> HandlerMap;
  HandlerMap handlers_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_RENDERER_XWALK_EXTENSION_CLIENT_H_
