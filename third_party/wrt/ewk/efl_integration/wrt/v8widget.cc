// Copyright 2014-2018,  Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrt/v8widget.h"

#include "base/logging.h"
#include "wrt/dynamicplugin.h"
#include "wrt/wrtwidget.h"

#if defined(OS_TIZEN_TV_PRODUCT)
#include "wrt/hbbtv_widget.h"
#endif

// static
V8Widget* V8Widget::CreateWidget(Type type,
                                 const base::CommandLine& command_line) {
  return new WrtWidget(command_line);
}

void V8Widget::StartSession(v8::Handle<v8::Context> context,
                            int routing_handle,
                            const char* session_blob) {
  if (plugin_ && !id_.empty() && !context.IsEmpty())
    plugin_->StartSession(id_.c_str(), context, routing_handle, session_blob);
}

void V8Widget::StopSession(v8::Handle<v8::Context> context) {
  if (plugin_ && !id_.empty() && !context.IsEmpty())
    plugin_->StopSession(id_.c_str(), context);
}
