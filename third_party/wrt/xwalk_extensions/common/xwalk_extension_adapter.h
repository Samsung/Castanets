// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_XWALK_EXTENSION_ADAPTER_H_
#define XWALK_EXTENSIONS_XWALK_EXTENSION_ADAPTER_H_

#include <map>

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_instance.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_EntryPoints.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_Permissions.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_Runtime.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_SyncMessage.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_Message_2.h"

namespace extensions {

class XWalkExtensionAdapter {
 public:
  typedef std::map<XW_Extension, XWalkExtension*> ExtensionMap;
  typedef std::map<XW_Instance, XWalkExtensionInstance*> InstanceMap;

  static XWalkExtensionAdapter* GetInstance();

  XW_Extension GetNextXWExtension();
  XW_Instance GetNextXWInstance();

  void RegisterExtension(XWalkExtension* extension);
  void UnregisterExtension(XWalkExtension* extension);

  void RegisterInstance(XWalkExtensionInstance* instance);
  void UnregisterInstance(XWalkExtensionInstance* instance);

  // Returns the correct struct according to interface asked. This is
  // passed to external extensions in XW_Initialize() call.
  static const void* GetInterface(const char* name);

 private:
  XWalkExtensionAdapter();
  virtual ~XWalkExtensionAdapter();

  static XWalkExtension* GetExtension(XW_Extension xw_extension);
  static XWalkExtensionInstance* GetExtensionInstance(XW_Instance xw_instance);

  static void CoreSetExtensionName(
      XW_Extension xw_extension, const char* name);
  static void CoreSetJavaScriptAPI(
      XW_Extension xw_extension, const char* javascript_api);
  static void CoreRegisterInstanceCallbacks(
      XW_Extension xw_extension,
      XW_CreatedInstanceCallback created,
      XW_DestroyedInstanceCallback destroyed);
  static void CoreRegisterShutdownCallback(
      XW_Extension xw_extension,
      XW_ShutdownCallback shutdown);
  static void CoreSetInstanceData(
      XW_Instance xw_instance, void* data);
  static void* CoreGetInstanceData(XW_Instance xw_instance);
  static void MessagingRegister(
      XW_Extension xw_extension,
      XW_HandleMessageCallback handle_message);
  static void MessagingPostMessage(
      XW_Instance xw_instance, const char* message);
  static void SyncMessagingRegister(
      XW_Extension xw_extension,
      XW_HandleSyncMessageCallback handle_sync_message);
  static void SyncMessagingSetSyncReply(
      XW_Instance xw_instance, const char* reply);
  static void EntryPointsSetExtraJSEntryPoints(
      XW_Extension xw_extension, const char** entry_points);
  static void RuntimeGetStringVariable(
      XW_Extension xw_extension,
      const char* key, char* value, unsigned int value_len);
  static int PermissionsCheckAPIAccessControl(
      XW_Extension xw_extension, const char* api_name);
  static int PermissionsRegisterPermissions(
      XW_Extension xw_extension, const char* perm_table);
  static void MessagingRegisterBinaryMessageCallback(
      XW_Extension xw_extension, XW_HandleBinaryMessageCallback handle_message);
  static void MessagingPostBinaryMessage(
      XW_Instance xw_instance, const char* message, size_t size);

  ExtensionMap extension_map_;
  InstanceMap instance_map_;

  XW_Extension next_xw_extension_;
  XW_Instance next_xw_instance_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_XWALK_EXTENSION_ADAPTER_H_
