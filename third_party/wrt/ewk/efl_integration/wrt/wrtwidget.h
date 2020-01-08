// Copyright 2014, 2015 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EWK_EFL_INTEGRATION_WRT_WRTWIDGET_H_
#define EWK_EFL_INTEGRATION_WRT_WRTWIDGET_H_

#include <string>

#include "content/public/renderer/render_thread_observer.h"
#include "third_party/wrt/ewk/efl_integration/common/content_switches_efl.h"
#include "third_party/wrt/ewk/efl_integration/wrt/v8widget.h"
#include "url/gurl.h"

class Ewk_Wrt_Message_Data;

class WrtRenderThreadObserver;

// Have to be created on the RenderThread.
class WrtWidget : public V8Widget {
 public:
  WrtWidget(const base::CommandLine& command_line);
  ~WrtWidget() override;

  content::RenderThreadObserver* GetObserver() override;
  void StartSession(v8::Handle<v8::Context>,
                    int routing_handle,
                    const char* session_blob) override;

  void SetWidgetInfo(const std::string& tizen_app_id,
                     const std::string& scale_factor,
                     const std::string& theme,
                     const std::string& encodedBundle);

  bool IsWidgetInfoSet() const;

  void MessageReceived(const Ewk_Wrt_Message_Data& data);

 private:
  // TODO(is46.kim) Remove |scale_|, |encoded_bundle_|, |theme_|
  double scale_;
  std::string encoded_bundle_;
  std::string theme_;
  std::unique_ptr<WrtRenderThreadObserver> observer_;
};

#endif  // EWK_EFL_INTEGRATION_WRT_WRTWIDGET_H_
