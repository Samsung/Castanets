// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_instance.h"

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_adapter.h"
#include "third_party/wrt/xwalk_extensions/public/XW_Extension_SyncMessage.h"

namespace extensions {
/*
XWalkExtensionInstance::XWalkExtensionInstance(
    XWalkExtension* extension, XW_Instance xw_instance)
  : extension_(extension),
    xw_instance_(xw_instance),
    instance_data_(NULL) {
  XWalkExtensionAdapter::GetInstance()->RegisterInstance(this);
  XW_CreatedInstanceCallback callback = extension_->created_instance_callback_;
  if (callback)
    callback(xw_instance_);
}

XWalkExtensionInstance::~XWalkExtensionInstance() {
  XW_DestroyedInstanceCallback callback =
      extension_->destroyed_instance_callback_;
  if (callback)
    callback(xw_instance_);
  XWalkExtensionAdapter::GetInstance()->UnregisterInstance(this);
}

void XWalkExtensionInstance::HandleMessage(const std::string& msg) {
  XW_HandleMessageCallback callback = extension_->handle_msg_callback_;
  if (callback)
    callback(xw_instance_, msg.c_str());
}

void XWalkExtensionInstance::HandleSyncMessage(const std::string& msg) {
  XW_HandleSyncMessageCallback callback = extension_->handle_sync_msg_callback_;
  if (callback) {
    callback(xw_instance_, msg.c_str());
  }
}
*/
void XWalkExtensionInstance::SetPostMessageCallback(
    MessageCallback callback) {
  post_message_callback_ = callback;
}

void XWalkExtensionInstance::SetSendSyncReplyCallback(
    MessageCallback callback) {
  send_sync_reply_callback_ = callback;
}

void XWalkExtensionInstance::PostMessageToJS(const std::string& msg) {
  post_message_callback_(msg);
}

void XWalkExtensionInstance::SyncReplyToJS(const std::string& reply) {
  send_sync_reply_callback_(reply);
}

}  // namespace extensions
