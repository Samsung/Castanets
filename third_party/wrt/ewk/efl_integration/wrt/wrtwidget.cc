// Copyright 2014, 2015 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrt/wrtwidget.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "common/render_messages_ewk.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_channel.h"
#include "wrt/wrt_dynamicplugin.h"

// TODO(z.kostrzewa)
// Why it can't be implemented as IPC::ChannelProxy::MessageFilter (?)
// Tried that and it seems that Observer starts receiving messages earlier than
// MessageFilter what is crucial for message that sets widget handle
class WrtRenderThreadObserver : public content::RenderThreadObserver {
 public:
  explicit WrtRenderThreadObserver(WrtWidget* wrt_widget)
    : wrt_widget_(wrt_widget),
      channel_(content::RenderThread::Get()->GetChannel())
  { }

  bool OnControlMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(WrtRenderThreadObserver, message)
      IPC_MESSAGE_FORWARD(WrtMsg_SendWrtMessage, wrt_widget_, WrtWidget::MessageReceived)
      IPC_MESSAGE_HANDLER(WrtMsg_ParseUrl, ParseUrl)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
 }

 private:
  void ParseUrl(int request_id, const GURL& url) {
    GURL response;
    wrt_widget_->ParseUrl(url, response);
    Send(new WrtMsg_ParseUrlResponse(request_id, response));
  }

  void Send(IPC::Message* message) {
    if (channel_)
      channel_->Send(message);
    else
      delete message;
  }

  WrtWidget* wrt_widget_;
  IPC::SyncChannel* channel_;
};

WrtWidget::WrtWidget(const base::CommandLine& command_line)
    : V8Widget(V8Widget::Type::WRT), scale_(0) {
  DCHECK(content::RenderThread::Get())
      << "WrtWidget must be constructed on the render thread";

  WrtDynamicPlugin::Get().InitRenderer();
  SetPlugin(&WrtDynamicPlugin::Get());

  observer_.reset(new WrtRenderThreadObserver(this));

  SetWidgetInfo(
      command_line.GetSwitchValueASCII(switches::kTizenAppId),
      command_line.GetSwitchValueASCII(switches::kWidgetScale),
      command_line.GetSwitchValueASCII(switches::kWidgetTheme),
      command_line.GetSwitchValueASCII(switches::kWidgetEncodedBundle));
}

WrtWidget::~WrtWidget() {}

content::RenderThreadObserver* WrtWidget::GetObserver() {
  return observer_.get();
}

void WrtWidget::StartSession(v8::Handle<v8::Context> context,
                             int routing_handle,
                             const char* session_blob) {
  if (!id_.empty() && !context.IsEmpty())
    WrtDynamicPlugin::Get().StartSession(
        id_.c_str(), context, routing_handle, session_blob, scale_,
        encoded_bundle_.c_str(), theme_.c_str());
}

void WrtWidget::SetWidgetInfo(const std::string& tizen_app_id,
                              const std::string& scale_factor,
                              const std::string& theme,
                              const std::string& encoded_bundle) {
  double scale = 1;
  base::StringToDouble(scale_factor, &scale);

  id_ = tizen_app_id;
  scale_ = scale;
  theme_ = theme;
  encoded_bundle_ = encoded_bundle;
  WrtDynamicPlugin::Get().SetWidgetInfo(id_);
}

bool WrtWidget::IsWidgetInfoSet() const {
  return !id_.empty();
}

void WrtWidget::MessageReceived(const Ewk_Wrt_Message_Data& data) {
  if (!id_.empty())
    WrtDynamicPlugin::Get().MessageReceived(data);
}
