// Copyright 2014, 2016 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EWK_EFL_INTEGRATION_WRT_DYNAMICPLUGIN_H_
#define EWK_EFL_INTEGRATION_WRT_DYNAMICPLUGIN_H_

#include <string>
#include "base/macros.h"
#include "v8/include/v8.h"
#include "wrt/v8widget.h"

class DynamicPlugin {
 public:
  virtual ~DynamicPlugin();
  virtual bool Init();
  virtual bool InitRenderer();

  // Interface for WebApp URL Conversion
/* LCOV_EXCL_START */
  virtual void SetWidgetInfo(const std::string& tizen_app_id) {}
  virtual bool CanHandleParseUrl(const std::string& scheme) const {
    return false;
/* LCOV_EXCL_STOP */
  }
  virtual void ParseURL(std::string* old_url,
                        std::string* new_url,
                        const char* tizen_app_id,
                        bool* is_decrypted_file = nullptr) = 0;

  void StartSession(const char* session_id,
                    v8::Handle<v8::Context> context,
                    int routing_handle,
                    const char* session_blob,
                    double scale_factor = 1.0f,
                    const char* encoded_bundle = nullptr,
                    const char* theme = nullptr) const;
  void StopSession(const char* session_id,
                   v8::Handle<v8::Context> context) const;

  unsigned int DynamicPluginVersion() {
    return 1;
  }
  void DynamicPluginStartSession(
    const char*, v8::Handle<v8::Context>, int, const char*) const;
  void DynamicPluginStopSession(
    const char* tizen_id, v8::Handle<v8::Context> context) const;

  static DynamicPlugin& Get(V8Widget::Type type);

 protected:
  DynamicPlugin();

 private:
  unsigned int version_;
};

#endif  // EWK_EFL_INTEGRATION_WRT_DYNAMICPLUGIN_H_
