// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_XWALK_EXTENSION_INSTANCE_H_
#define XWALK_EXTENSIONS_XWALK_EXTENSION_INSTANCE_H_

#include <functional>
#include <string>

// #include "third_party/xwalk_extensions/public/XW_Extension.h"

namespace extensions {

class XWalkExtension;

class XWalkExtensionInstance {
 public:
  typedef std::function<void(const std::string&)> MessageCallback;
/*
  XWalkExtensionInstance(XWalkExtension* extension, XW_Instance xw_instance);
  virtual ~XWalkExtensionInstance();

  void HandleMessage(const std::string& msg);
  void HandleSyncMessage(const std::string& msg);
*/
  void SetPostMessageCallback(MessageCallback callback);
  void SetSendSyncReplyCallback(MessageCallback callback);

 private:
  friend class XWalkExtensionAdapter;

  void PostMessageToJS(const std::string& msg);
  void SyncReplyToJS(const std::string& reply);

  XWalkExtension* extension_;
  // XW_Instance xw_instance_;
  void* instance_data_;

  MessageCallback post_message_callback_;
  MessageCallback send_sync_reply_callback_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_XWALK_EXTENSION_INSTANCE_H_
