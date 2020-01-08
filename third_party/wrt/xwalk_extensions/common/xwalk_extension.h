// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_XWALK_EXTENSION_H_
#define XWALK_EXTENSIONS_XWALK_EXTENSION_H_

#include <string>
#include <vector>

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_instance.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_SyncMessage.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_Message_2.h"

namespace extensions {

class XWalkExtensionAdapter;
class XWalkExtensionInstance;

class XWalkExtension {
 public:
  typedef std::vector<std::string> StringVector;

  class XWalkExtensionDelegate {
   public:
    virtual void GetRuntimeVariable(const char* key, char* value,
        size_t value_len) = 0;
  };

  XWalkExtension(const std::string& path, XWalkExtensionDelegate* delegate);
  XWalkExtension(const std::string& path,
                 const std::string& name,
                 const StringVector& entry_points,
                 XWalkExtensionDelegate* delegate);
  virtual ~XWalkExtension();

  bool Initialize();
  // XWalkExtensionInstance* CreateInstance();
  std::string GetJavascriptCode();

  std::string name() const { return name_; }

  const StringVector& entry_points() const {
    return entry_points_;
  }

  bool lazy_loading() const {
    return lazy_loading_;
  }

 private:
  friend class XWalkExtensionAdapter;
  friend class XWalkExtensionInstance;

  void GetRuntimeVariable(const char* key, char* value, size_t value_len);
  int CheckAPIAccessControl(const char* api_name);
  int RegisterPermissions(const char* perm_table);

  bool initialized_;
  std::string library_path_;
  XW_Extension xw_extension_;

  std::string name_;
  std::string javascript_api_;
  StringVector entry_points_;
  bool lazy_loading_;

  XWalkExtensionDelegate* delegate_;

  XW_CreatedInstanceCallback created_instance_callback_;
  XW_DestroyedInstanceCallback destroyed_instance_callback_;
  XW_ShutdownCallback shutdown_callback_;
  XW_HandleMessageCallback handle_msg_callback_;
  XW_HandleSyncMessageCallback handle_sync_msg_callback_;
  XW_HandleBinaryMessageCallback handle_binary_msg_callback_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_XWALK_EXTENSION_H_
