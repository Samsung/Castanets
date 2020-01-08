// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_XWALK_EXTENSION_SERVER_H_
#define XWALK_EXTENSIONS_XWALK_EXTENSION_SERVER_H_

// #include <Eina.h>
#include <json/json.h>
#include <string>
#include <map>

#include "base/synchronization/lock.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_manager.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_instance.h"

typedef struct Ewk_Wrt_Message_Data Ewk_IPC_Wrt_Message_Data;

namespace extensions {

class XWalkExtensionServer {
 public:
  static XWalkExtensionServer* GetInstance();

  void Preload();
  /*
  Json::Value GetExtensions();
  std::string GetAPIScript(const std::string& extension_name);
  std::string CreateInstance(const std::string& extension_name);

  void HandleIPCMessage(Ewk_IPC_Wrt_Message_Data* data);
  */
  void Shutdown();

  void LoadUserExtensions(const std::string app_path);
  /*
  void SendWrtMessage(
      Eina_Stringshare* type, Eina_Stringshare* id, const char* val);
  */
 private:
  XWalkExtensionServer();
  virtual ~XWalkExtensionServer();
/*
  void HandleGetExtensions(Ewk_IPC_Wrt_Message_Data* data);
  void HandleCreateInstance(Ewk_IPC_Wrt_Message_Data* data);
  void HandleDestroyInstance(Ewk_IPC_Wrt_Message_Data* data);
  void HandlePostMessageToNative(Ewk_IPC_Wrt_Message_Data* data);
  void HandleSendSyncMessageToNative(Ewk_IPC_Wrt_Message_Data* data);
  void HandleGetAPIScript(Ewk_IPC_Wrt_Message_Data* data);
  */
  typedef std::map<std::string, XWalkExtensionInstance*> InstanceMap;

  base::Lock extension_server_lock_;
  XWalkExtensionManager manager_;
  InstanceMap instances_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_XWALK_EXTENSION_SERVER_H_
