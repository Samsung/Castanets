// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/text_input_test_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/mac/attributed_string_type_converters.h"
#include "content/common/text_input_client_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/mojom/attributed_string.mojom.h"

namespace content {

TestTextInputClientMessageFilter::TestTextInputClientMessageFilter(
    RenderProcessHost* host)
    : BrowserMessageFilter(TextInputClientMsgStart),
      host_(host),
      received_string_from_range_(false) {
  host->AddFilter(this);
}

TestTextInputClientMessageFilter::~TestTextInputClientMessageFilter() {}

void TestTextInputClientMessageFilter::WaitForStringFromRange() {
  if (received_string_from_range_)
    return;
  message_loop_runner_ = new MessageLoopRunner();
  message_loop_runner_->Run();
}

bool TestTextInputClientMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (message.type() == TextInputClientReplyMsg_GotStringForRange::ID) {
    if (!string_for_range_callback_.is_null()) {
      GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                          string_for_range_callback_);
    }

    received_string_from_range_ = true;

    // Now decode the string to get the word.
    TextInputClientReplyMsg_GotStringForRange::Param params;
    TextInputClientReplyMsg_GotStringForRange::Read(&message, &params);
    const ui::mojom::AttributedString& attributed_string = std::get<0>(params);
    ui::mojom::AttributedStringPtr string = attributed_string.Clone();
    NSAttributedString* ns_attributed_string = string.To<NSAttributedString*>();
    string_from_range_ = base::SysNSStringToUTF8([ns_attributed_string string]);

    // Stop the message loop if it is running.
    if (message_loop_runner_) {
      GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                          message_loop_runner_->QuitClosure());
    }
  }

  // unhandled - leave it for the actual TextInputClientMessageFilter to handle.
  return false;
}

void TestTextInputClientMessageFilter::SetStringForRangeCallback(
    base::RepeatingClosure callback) {
  string_for_range_callback_ = std::move(callback);
}

void AskForLookUpDictionaryForRange(RenderWidgetHostView* tab_view,
                                    const gfx::Range& range) {
  RenderWidgetHostViewMac* tab_view_mac =
      static_cast<RenderWidgetHostViewMac*>(tab_view);
  tab_view_mac->LookUpDictionaryOverlayFromRange(range);
}

size_t GetOpenNSWindowsCount() {
  return [[NSApp windows] count];
}

}  // namespace content
