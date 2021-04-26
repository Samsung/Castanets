// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_IMPL_H_
#define CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"

namespace content {
class MainThreadEventQueue;
class RenderWidget;
class WidgetInputHandlerManager;

// This class provides an implementation of the mojo WidgetInputHandler
// interface. If threaded compositing is used this thread will live on
// the compositor thread and proxy events to the main thread. This
// is done so that events stay in order relative to other events.
class WidgetInputHandlerImpl : public blink::mojom::WidgetInputHandler {
 public:
  WidgetInputHandlerImpl(
      scoped_refptr<WidgetInputHandlerManager> manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      scoped_refptr<MainThreadEventQueue> input_event_queue,
      base::WeakPtr<RenderWidget> render_widget);
  ~WidgetInputHandlerImpl() override;

  void SetAssociatedReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetInputHandler>
          interface_receiver);
  void SetReceiver(mojo::PendingReceiver<blink::mojom::WidgetInputHandler>
                       interface_receiver);

  void SetFocus(bool focused) override;
  void MouseCaptureLost() override;
  void SetEditCommandsForNextKeyEvent(
      std::vector<blink::mojom::EditCommandPtr> commands) override;
  void CursorVisibilityChanged(bool visible) override;
  void ImeSetComposition(const base::string16& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& range,
                         int32_t start,
                         int32_t end) override;
  void ImeCommitText(const base::string16& text,
                     const std::vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& range,
                     int32_t relative_cursor_position,
                     ImeCommitTextCallback callback) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void RequestTextInputStateUpdate() override;
  void RequestCompositionUpdates(bool immediate_request,
                                 bool monitor_request) override;
  void DispatchEvent(std::unique_ptr<blink::WebCoalescedInputEvent>,
                     DispatchEventCallback callback) override;
  void DispatchNonBlockingEvent(
      std::unique_ptr<blink::WebCoalescedInputEvent>) override;
  void WaitForInputProcessed(WaitForInputProcessedCallback callback) override;
  void AttachSynchronousCompositor(
      mojo::PendingRemote<blink::mojom::SynchronousCompositorControlHost>
          control_host,
      mojo::PendingAssociatedRemote<blink::mojom::SynchronousCompositorHost>
          host,
      mojo::PendingAssociatedReceiver<blink::mojom::SynchronousCompositor>
          compositor_receiver) override;
  void GetFrameWidgetInputHandler(
      mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetInputHandler>
          interface_request) override;
  void InputWasProcessed();

 private:
  bool ShouldProxyToMainThread() const;
  void RunOnMainThread(base::OnceClosure closure);
  void Release();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<WidgetInputHandlerManager> input_handler_manager_;
  scoped_refptr<MainThreadEventQueue> input_event_queue_;
  base::WeakPtr<RenderWidget> render_widget_;

  // This callback is used to respond to the WaitForInputProcessed Mojo
  // message. We keep it around so that we can respond even if the renderer is
  // killed before we actually fully process the input.
  WaitForInputProcessedCallback input_processed_ack_;

  mojo::Receiver<blink::mojom::WidgetInputHandler> receiver_{this};

  base::WeakPtrFactory<WidgetInputHandlerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WidgetInputHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_IMPL_H_
