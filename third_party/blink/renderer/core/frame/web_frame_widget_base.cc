// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace WTF {
template <>
struct CrossThreadCopier<blink::WebReportTimeCallback>
    : public CrossThreadCopierByValuePassThrough<blink::WebReportTimeCallback> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
STATIC_ASSERT_ENUM(kDragOperationNone, kWebDragOperationNone);
STATIC_ASSERT_ENUM(kDragOperationCopy, kWebDragOperationCopy);
STATIC_ASSERT_ENUM(kDragOperationLink, kWebDragOperationLink);
STATIC_ASSERT_ENUM(kDragOperationGeneric, kWebDragOperationGeneric);
STATIC_ASSERT_ENUM(kDragOperationPrivate, kWebDragOperationPrivate);
STATIC_ASSERT_ENUM(kDragOperationMove, kWebDragOperationMove);
STATIC_ASSERT_ENUM(kDragOperationDelete, kWebDragOperationDelete);
STATIC_ASSERT_ENUM(kDragOperationEvery, kWebDragOperationEvery);

bool WebFrameWidgetBase::ignore_input_events_ = false;

WebFrameWidgetBase::WebFrameWidgetBase(
    WebWidgetClient& client,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget)
    : widget_base_(std::make_unique<WidgetBase>(this,
                                                std::move(widget_host),
                                                std::move(widget))),
      client_(&client) {
  frame_widget_host_.Bind(std::move(frame_widget_host),
                          ThreadScheduler::Current()->IPCTaskRunner());
  receiver_.Bind(std::move(frame_widget),
                 ThreadScheduler::Current()->IPCTaskRunner());
}

WebFrameWidgetBase::~WebFrameWidgetBase() = default;

void WebFrameWidgetBase::BindLocalRoot(WebLocalFrame& local_root) {
  local_root_ = To<WebLocalFrameImpl>(local_root);
  local_root_->SetFrameWidget(this);
  request_animation_after_delay_timer_.reset(
      new TaskRunnerTimer<WebFrameWidgetBase>(
          local_root.GetTaskRunner(TaskType::kInternalDefault), this,
          &WebFrameWidgetBase::RequestAnimationAfterDelayTimerFired));
}

void WebFrameWidgetBase::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
    base::OnceCallback<void()> cleanup_task) {
  mutator_dispatcher_ = nullptr;
  local_root_->SetFrameWidget(nullptr);
  local_root_ = nullptr;
  client_ = nullptr;
  request_animation_after_delay_timer_.reset();
  widget_base_->Shutdown(std::move(cleanup_runner), std::move(cleanup_task));
  widget_base_.reset();
  receiver_.reset();
}

WebLocalFrame* WebFrameWidgetBase::LocalRoot() const {
  return local_root_;
}

WebRect WebFrameWidgetBase::ComputeBlockBound(
    const gfx::Point& point_in_root_frame,
    bool ignore_clipping) const {
  HitTestLocation location(local_root_->GetFrameView()->ConvertFromRootFrame(
      PhysicalOffset(IntPoint(point_in_root_frame))));
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kReadOnly | HitTestRequest::kActive |
      (ignore_clipping ? HitTestRequest::kIgnoreClipping : 0);
  HitTestResult result =
      local_root_->GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location, hit_type);
  result.SetToShadowHostIfInRestrictedShadowRoot();

  Node* node = result.InnerNodeOrImageMapImage();
  if (!node)
    return WebRect();

  // Find the block type node based on the hit node.
  // FIXME: This wants to walk flat tree with
  // LayoutTreeBuilderTraversal::parent().
  while (node &&
         (!node->GetLayoutObject() || node->GetLayoutObject()->IsInline()))
    node = LayoutTreeBuilderTraversal::Parent(*node);

  // Return the bounding box in the root frame's coordinate space.
  if (node) {
    IntRect absolute_rect = node->GetLayoutObject()->AbsoluteBoundingBoxRect();
    LocalFrame* frame = node->GetDocument().GetFrame();
    return frame->View()->ConvertToRootFrame(absolute_rect);
  }
  return WebRect();
}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnter(
    const WebDragData& web_drag_data,
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  DCHECK(!current_drag_data_);

  current_drag_data_ = DataObject::Create(web_drag_data);
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragEnter,
                                   modifiers);
}

void WebFrameWidgetBase::DragTargetDragOver(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    WebDragOperationsMask operations_allowed,
    uint32_t modifiers,
    DragTargetDragOverCallback callback) {
  operations_allowed_ = operations_allowed;

  blink::WebDragOperation operation = DragTargetDragEnterOrOver(
      point_in_viewport, screen_point, kDragOver, modifiers);
  std::move(callback).Run(operation);
}

void WebFrameWidgetBase::DragTargetDragLeave(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point) {
  DCHECK(current_drag_data_);

  // TODO(paulmeyer): It shouldn't be possible for |current_drag_data_| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-leave is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!current_drag_data_| should be removed. (crbug.com/671152)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return;
  }

  gfx::PointF point_in_root_frame(ViewportToRootFrame(point_in_viewport));
  DragData drag_data(current_drag_data_.Get(), FloatPoint(point_in_root_frame),
                     FloatPoint(screen_point),
                     static_cast<DragOperation>(operations_allowed_));

  GetPage()->GetDragController().DragExited(&drag_data,
                                            *local_root_->GetFrame());

  // FIXME: why is the drag scroll timer not stopped here?

  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragTargetDrop(const WebDragData& web_drag_data,
                                        const gfx::PointF& point_in_viewport,
                                        const gfx::PointF& screen_point,
                                        int modifiers) {
  gfx::PointF point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  DCHECK(current_drag_data_);
  current_drag_data_ = DataObject::Create(web_drag_data);

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview m_dragOperation state is not DragOperationNone.

  if (drag_operation_ == kWebDragOperationNone) {
    // IPC RACE CONDITION: do not allow this drop.
    DragTargetDragLeave(point_in_viewport, screen_point);
    return;
  }

  if (!IgnoreInputEvents()) {
    current_drag_data_->SetModifiers(modifiers);
    DragData drag_data(current_drag_data_.Get(),
                       FloatPoint(point_in_root_frame),
                       FloatPoint(screen_point),
                       static_cast<DragOperation>(operations_allowed_));

    GetPage()->GetDragController().PerformDrag(&drag_data,
                                               *local_root_->GetFrame());
  }
  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragSourceEndedAt(const gfx::PointF& point_in_viewport,
                                           const gfx::PointF& screen_point,
                                           WebDragOperation operation) {
  if (!local_root_) {
    // We should figure out why |local_root_| could be nullptr
    // (https://crbug.com/792345).
    return;
  }

  if (IgnoreInputEvents()) {
    CancelDrag();
    return;
  }
  gfx::PointF point_in_root_frame(
      GetPage()->GetVisualViewport().ViewportToRootFrame(
          FloatPoint(point_in_viewport)));

  WebMouseEvent fake_mouse_move(
      WebInputEvent::Type::kMouseMove, point_in_root_frame, screen_point,
      WebPointerProperties::Button::kLeft, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  fake_mouse_move.SetFrameScale(1);
  local_root_->GetFrame()->GetEventHandler().DragSourceEndedAt(
      fake_mouse_move, static_cast<DragOperation>(operation));
}

void WebFrameWidgetBase::DragSourceSystemDragEnded() {
  CancelDrag();
}

void WebFrameWidgetBase::SetBackgroundOpaque(bool opaque) {
  if (opaque) {
    View()->ClearBaseBackgroundColorOverride();
    View()->ClearBackgroundColorOverride();
  } else {
    View()->SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
    View()->SetBackgroundColorOverride(SK_ColorTRANSPARENT);
  }
}

void WebFrameWidgetBase::SetTextDirection(base::i18n::TextDirection direction) {
  LocalFrame* focusedFrame = FocusedLocalFrameInWidget();
  if (focusedFrame)
    focusedFrame->SetTextDirection(direction);
}

void WebFrameWidgetBase::CancelDrag() {
  // It's possible for this to be called while we're not doing a drag if
  // it's from a previous page that got unloaded.
  if (!doing_drag_and_drop_)
    return;
  GetPage()->GetDragController().DragEnded();
  doing_drag_and_drop_ = false;
}

void WebFrameWidgetBase::StartDragging(network::mojom::ReferrerPolicy policy,
                                       const WebDragData& data,
                                       WebDragOperationsMask mask,
                                       const SkBitmap& drag_image,
                                       const gfx::Point& drag_image_offset) {
  doing_drag_and_drop_ = true;
  Client()->StartDragging(policy, data, mask, drag_image, drag_image_offset);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnterOrOver(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    DragAction drag_action,
    int modifiers) {
  DCHECK(current_drag_data_);
  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-over is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671504)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return kWebDragOperationNone;
  }

  FloatPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  current_drag_data_->SetModifiers(modifiers);
  DragData drag_data(current_drag_data_.Get(), FloatPoint(point_in_root_frame),
                     FloatPoint(screen_point),
                     static_cast<DragOperation>(operations_allowed_));

  DragOperation drag_operation =
      GetPage()->GetDragController().DragEnteredOrUpdated(
          &drag_data, *local_root_->GetFrame());

  // Mask the drag operation against the drag source's allowed
  // operations.
  if (!(drag_operation & drag_data.DraggingSourceOperationMask()))
    drag_operation = kDragOperationNone;

  drag_operation_ = static_cast<WebDragOperation>(drag_operation);

  return drag_operation_;
}

void WebFrameWidgetBase::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node = View()->FindNodeFromScrollableCompositorElementId(
      scroll_latched_element_id);
  if (target_node) {
    target_node->GetDocument().EnqueueOverscrollEventForNode(
        target_node, overscroll_delta.x(), overscroll_delta.y());
  }
}

void WebFrameWidgetBase::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node = View()->FindNodeFromScrollableCompositorElementId(
      scroll_latched_element_id);
  if (target_node)
    target_node->GetDocument().EnqueueScrollEndEventForNode(target_node);
}

gfx::PointF WebFrameWidgetBase::ViewportToRootFrame(
    const gfx::PointF& point_in_viewport) const {
  return GetPage()->GetVisualViewport().ViewportToRootFrame(
      FloatPoint(point_in_viewport));
}

WebViewImpl* WebFrameWidgetBase::View() const {
  return local_root_->ViewImpl();
}

Page* WebFrameWidgetBase::GetPage() const {
  return View()->GetPage();
}

mojom::blink::FrameWidgetHost*
WebFrameWidgetBase::GetAssociatedFrameWidgetHost() const {
  return frame_widget_host_.get();
}

void WebFrameWidgetBase::DidAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidAcquirePointerLock();

  LocalFrame* focusedFrame = FocusedLocalFrameInWidget();
  if (focusedFrame) {
    focusedFrame->GetEventHandler().ReleaseMousePointerCapture();
  }
}

void WebFrameWidgetBase::DidNotAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidNotAcquirePointerLock();
}

void WebFrameWidgetBase::DidLosePointerLock() {
  GetPage()->GetPointerLockController().DidLosePointerLock();
}

void WebFrameWidgetBase::RequestDecode(
    const PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  Client()->RequestDecode(image, std::move(callback));
}

void WebFrameWidgetBase::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(current_drag_data_);
  visitor->Trace(frame_widget_host_);
  visitor->Trace(receiver_);
}

void WebFrameWidgetBase::SetNeedsRecalculateRasterScales() {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->SetNeedsRecalculateRasterScales();
}

void WebFrameWidgetBase::SetBackgroundColor(SkColor color) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->set_background_color(color);
}

void WebFrameWidgetBase::SetOverscrollBehavior(
    const cc::OverscrollBehavior& overscroll_behavior) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->SetOverscrollBehavior(overscroll_behavior);
}

void WebFrameWidgetBase::RegisterSelection(cc::LayerSelection selection) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->RegisterSelection(selection);
}

void WebFrameWidgetBase::StartPageScaleAnimation(
    const gfx::Vector2d& destination,
    bool use_anchor,
    float new_page_scale,
    base::TimeDelta duration) {
  widget_base_->LayerTreeHost()->StartPageScaleAnimation(
      destination, use_anchor, new_page_scale, duration);
}

void WebFrameWidgetBase::RequestBeginMainFrameNotExpected(bool request) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->RequestBeginMainFrameNotExpected(request);
}

void WebFrameWidgetBase::EndCommitCompositorFrame(
    base::TimeTicks commit_start_time) {
  Client()->DidCommitCompositorFrame(commit_start_time);
}

void WebFrameWidgetBase::DidCommitAndDrawCompositorFrame() {
  Client()->DidCommitAndDrawCompositorFrame();
}

void WebFrameWidgetBase::DidObserveFirstScrollDelay(
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  if (!local_root_ || !(local_root_->GetFrame()) ||
      !(local_root_->GetFrame()->GetDocument())) {
    return;
  }
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*(local_root_->GetFrame()->GetDocument()));
  if (interactive_detector) {
    interactive_detector->DidObserveFirstScrollDelay(first_scroll_delay,
                                                     first_scroll_timestamp);
  }
}

void WebFrameWidgetBase::OnDeferMainFrameUpdatesChanged(bool defer) {
  Client()->OnDeferMainFrameUpdatesChanged(defer);
}

void WebFrameWidgetBase::OnDeferCommitsChanged(bool defer) {
  Client()->OnDeferCommitsChanged(defer);
}

void WebFrameWidgetBase::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  Client()->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WebFrameWidgetBase::DidCompletePageScaleAnimation() {
  Client()->DidCompletePageScaleAnimation();
}

void WebFrameWidgetBase::DidBeginMainFrame() {
  Client()->DidBeginMainFrame();
}

void WebFrameWidgetBase::WillBeginMainFrame() {
  Client()->WillBeginMainFrame();
}

void WebFrameWidgetBase::SubmitThroughputData(
    ukm::SourceId source_id,
    int aggregated_percent,
    int impl_percent,
    base::Optional<int> main_percent) {
  local_root_->Client()->SubmitThroughputData(source_id, aggregated_percent,
                                              impl_percent, main_percent);
}

void WebFrameWidgetBase::ScheduleAnimationForWebTests() {
  Client()->ScheduleAnimationForWebTests();
}

int WebFrameWidgetBase::GetLayerTreeId() {
  if (!View()->does_composite())
    return 0;
  return widget_base_->LayerTreeHost()->GetId();
}

void WebFrameWidgetBase::SetHaveScrollEventHandlers(bool has_handlers) {
  widget_base_->LayerTreeHost()->SetHaveScrollEventHandlers(has_handlers);
}

void WebFrameWidgetBase::SetEventListenerProperties(
    cc::EventListenerClass listener_class,
    cc::EventListenerProperties listener_properties) {
  widget_base_->LayerTreeHost()->SetEventListenerProperties(
      listener_class, listener_properties);

  if (listener_class == cc::EventListenerClass::kTouchStartOrMove ||
      listener_class == cc::EventListenerClass::kTouchEndOrCancel) {
    bool has_touch_handlers =
        EventListenerProperties(cc::EventListenerClass::kTouchStartOrMove) !=
            cc::EventListenerProperties::kNone ||
        EventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel) !=
            cc::EventListenerProperties::kNone;
    if (!has_touch_handlers_ || *has_touch_handlers_ != has_touch_handlers) {
      has_touch_handlers_ = has_touch_handlers;

      // Can be NULL when running tests.
      if (auto* scheduler_state = widget_base_->RendererWidgetSchedulingState())
        scheduler_state->SetHasTouchHandler(has_touch_handlers);
      frame_widget_host_->SetHasTouchEventHandlers(has_touch_handlers);
    }
  } else if (listener_class == cc::EventListenerClass::kPointerRawUpdate) {
    client_->SetHasPointerRawUpdateEventHandlers(
        listener_properties != cc::EventListenerProperties::kNone);
  }
}

cc::EventListenerProperties WebFrameWidgetBase::EventListenerProperties(
    cc::EventListenerClass listener_class) const {
  return widget_base_->LayerTreeHost()->event_listener_properties(
      listener_class);
}

mojom::blink::DisplayMode WebFrameWidgetBase::DisplayMode() const {
  return display_mode_;
}

const WebVector<WebRect>& WebFrameWidgetBase::WindowSegments() const {
  return window_segments_;
}

void WebFrameWidgetBase::StartDeferringCommits(base::TimeDelta timeout) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->StartDeferringCommits(timeout);
}

void WebFrameWidgetBase::StopDeferringCommits(
    cc::PaintHoldingCommitTrigger triggger) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->StopDeferringCommits(triggger);
}

std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
WebFrameWidgetBase::DeferMainFrameUpdate() {
  return widget_base_->LayerTreeHost()->DeferMainFrameUpdate();
}

void WebFrameWidgetBase::SetBrowserControlsShownRatio(float top_ratio,
                                                      float bottom_ratio) {
  widget_base_->LayerTreeHost()->SetBrowserControlsShownRatio(top_ratio,
                                                              bottom_ratio);
}

void WebFrameWidgetBase::SetBrowserControlsParams(
    cc::BrowserControlsParams params) {
  widget_base_->LayerTreeHost()->SetBrowserControlsParams(params);
}

cc::LayerTreeDebugState WebFrameWidgetBase::GetLayerTreeDebugState() {
  return widget_base_->LayerTreeHost()->GetDebugState();
}

void WebFrameWidgetBase::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& state) {
  widget_base_->LayerTreeHost()->SetDebugState(state);
}

void WebFrameWidgetBase::SynchronouslyCompositeForTesting(
    base::TimeTicks frame_time) {
  widget_base_->LayerTreeHost()->Composite(frame_time, false);
}

// TODO(665924): Remove direct dispatches of mouse events from
// PointerLockController, instead passing them through EventHandler.
void WebFrameWidgetBase::PointerLockMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  const WebMouseEvent& mouse_event =
      static_cast<const WebMouseEvent&>(input_event);
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root_->GetFrameView(), mouse_event);

  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::Type::kMouseDown:
      event_type = event_type_names::kMousedown;
      if (!GetPage() || !GetPage()->GetPointerLockController().GetElement())
        break;
      LocalFrame::NotifyUserActivation(GetPage()
                                           ->GetPointerLockController()
                                           .GetElement()
                                           ->GetDocument()
                                           .GetFrame());
      break;
    case WebInputEvent::Type::kMouseUp:
      event_type = event_type_names::kMouseup;
      break;
    case WebInputEvent::Type::kMouseMove:
      event_type = event_type_names::kMousemove;
      break;
    default:
      NOTREACHED() << input_event.GetType();
  }

  if (GetPage()) {
    GetPage()->GetPointerLockController().DispatchLockedMouseEvent(
        transformed_event,
        TransformWebMouseEventVector(
            local_root_->GetFrameView(),
            coalesced_event.GetCoalescedEventsPointers()),
        TransformWebMouseEventVector(
            local_root_->GetFrameView(),
            coalesced_event.GetPredictedEventsPointers()),
        event_type);
  }
}

void WebFrameWidgetBase::ShowContextMenu(WebMenuSourceType source_type) {
  if (!GetPage())
    return;

  GetPage()->GetContextMenuController().ClearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focused_frame =
            GetPage()->GetFocusController().FocusedFrame()) {
      focused_frame->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                                 source_type);
    }
  }
}

LocalFrame* WebFrameWidgetBase::FocusedLocalFrameInWidget() const {
  if (!local_root_) {
    // WebFrameWidget is created in the call to CreateFrame. The corresponding
    // RenderWidget, however, might not swap in right away (InstallNewDocument()
    // will lead to it swapping in). During this interval local_root_ is nullptr
    // (see https://crbug.com/792345).
    return nullptr;
  }

  LocalFrame* frame = GetPage()->GetFocusController().FocusedFrame();
  return (frame && frame->LocalFrameRoot() == local_root_->GetFrame())
             ? frame
             : nullptr;
}

WebLocalFrame* WebFrameWidgetBase::FocusedWebLocalFrameInWidget() const {
  return WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
}

cc::LayerTreeHost* WebFrameWidgetBase::InitializeCompositing(
    cc::TaskGraphRunner* task_graph_runner,
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  widget_base_->InitializeCompositing(task_graph_runner, settings,
                                      std::move(ukm_recorder_factory));
  GetPage()->AnimationHostInitialized(*AnimationHost(),
                                      GetLocalFrameViewForAnimationScrolling());
  return widget_base_->LayerTreeHost();
}

void WebFrameWidgetBase::SetCompositorVisible(bool visible) {
  widget_base_->SetCompositorVisible(visible);
}

void WebFrameWidgetBase::RecordTimeToFirstActivePaint(
    base::TimeDelta duration) {
  Client()->RecordTimeToFirstActivePaint(duration);
}

void WebFrameWidgetBase::DispatchRafAlignedInput(base::TimeTicks frame_time) {
  base::TimeTicks raf_aligned_input_start_time;
  if (LocalRootImpl() && WidgetBase::ShouldRecordBeginMainFrameMetrics()) {
    raf_aligned_input_start_time = base::TimeTicks::Now();
  }

  Client()->DispatchRafAlignedInput(frame_time);

  if (LocalRootImpl() && WidgetBase::ShouldRecordBeginMainFrameMetrics()) {
    LocalRootImpl()->GetFrame()->View()->EnsureUkmAggregator().RecordSample(
        LocalFrameUkmAggregator::kHandleInputEvents,
        raf_aligned_input_start_time, base::TimeTicks::Now());
  }
}

bool WebFrameWidgetBase::WillHandleGestureEvent(const WebGestureEvent& event) {
  return Client()->WillHandleGestureEvent(event);
}

bool WebFrameWidgetBase::WillHandleMouseEvent(const WebMouseEvent& event) {
  return Client()->WillHandleMouseEvent(event);
}

void WebFrameWidgetBase::ObserveGestureEventAndResult(
    const WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    const cc::OverscrollBehavior& overscroll_behavior,
    bool event_processed) {
  Client()->DidHandleGestureScrollEvent(gesture_event, unused_delta,
                                        overscroll_behavior, event_processed);
}

void WebFrameWidgetBase::DidHandleKeyEvent() {
  ClearEditCommands();
}

void WebFrameWidgetBase::QueueSyntheticEvent(
    std::unique_ptr<blink::WebCoalescedInputEvent> event) {
  Client()->QueueSyntheticEvent(std::move(event));
}

WebTextInputType WebFrameWidgetBase::GetTextInputType() {
  if (Client()->ShouldDispatchImeEventsToPepper())
    return Client()->GetPepperTextInputType();

  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return WebTextInputType::kWebTextInputTypeNone;
  return controller->TextInputType();
}

void WebFrameWidgetBase::GetWidgetInputHandler(
    mojo::PendingReceiver<mojom::blink::WidgetInputHandler> request,
    mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host) {
  Client()->GetWidgetInputHandler(std::move(request), std::move(host));
}

bool WebFrameWidgetBase::HasCurrentImeGuard(
    bool request_to_show_virtual_keyboard) {
  return Client()->HasCurrentImeGuard(request_to_show_virtual_keyboard);
}

void WebFrameWidgetBase::SendCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  Client()->SendCompositionRangeChanged(range, character_bounds);
}

void WebFrameWidgetBase::ApplyViewportChangesForTesting(
    const ApplyViewportChangesArgs& args) {
  widget_base_->ApplyViewportChanges(args);
}

void WebFrameWidgetBase::SetDisplayMode(mojom::blink::DisplayMode mode) {
  if (mode != display_mode_) {
    display_mode_ = mode;
    LocalFrame* frame = LocalRootImpl()->GetFrame();
    frame->MediaQueryAffectingValueChangedForLocalSubtree(
        MediaValueChange::kOther);
  }
}

void WebFrameWidgetBase::SetWindowSegments(WebVector<WebRect> window_segments) {
  if (!window_segments_.Equals(window_segments))
    window_segments_ = std::move(window_segments);
}

void WebFrameWidgetBase::SetCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
}

bool WebFrameWidgetBase::HandlingInputEvent() {
  return widget_base_->input_handler().handling_input_event();
}

void WebFrameWidgetBase::SetHandlingInputEvent(bool handling) {
  widget_base_->input_handler().set_handling_input_event(handling);
}

void WebFrameWidgetBase::ProcessInputEventSynchronously(
    const WebCoalescedInputEvent& event,
    HandledEventCallback callback) {
  widget_base_->input_handler().HandleInputEvent(event, std::move(callback));
}

void WebFrameWidgetBase::UpdateTextInputState() {
  widget_base_->UpdateTextInputState();
}

void WebFrameWidgetBase::ForceTextInputStateUpdate() {
  widget_base_->ForceTextInputStateUpdate();
}

void WebFrameWidgetBase::UpdateCompositionInfo() {
  widget_base_->UpdateCompositionInfo(/*immediate_request=*/false);
}

void WebFrameWidgetBase::UpdateSelectionBounds() {
  widget_base_->UpdateSelectionBounds();
}

void WebFrameWidgetBase::ShowVirtualKeyboard() {
  widget_base_->ShowVirtualKeyboard();
}

void WebFrameWidgetBase::RequestCompositionUpdates(bool immediate_request,
                                                   bool monitor_updates) {
  widget_base_->RequestCompositionUpdates(immediate_request, monitor_updates);
}

void WebFrameWidgetBase::AutoscrollStart(const gfx::PointF& position) {
  GetAssociatedFrameWidgetHost()->AutoscrollStart(std::move(position));
}

void WebFrameWidgetBase::AutoscrollFling(const gfx::Vector2dF& velocity) {
  GetAssociatedFrameWidgetHost()->AutoscrollFling(std::move(velocity));
}

void WebFrameWidgetBase::AutoscrollEnd() {
  GetAssociatedFrameWidgetHost()->AutoscrollEnd();
}

void WebFrameWidgetBase::DidMeaningfulLayout(WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::kVisuallyNonEmpty) {
    NotifySwapAndPresentationTime(
        base::NullCallback(),
        WTF::Bind(&WebFrameWidgetBase::PresentationCallbackForMeaningfulLayout,
                  WrapPersistent(this)));
  }

  if (client_)
    client_->DidMeaningfulLayout(layout_type);
}

void WebFrameWidgetBase::PresentationCallbackForMeaningfulLayout(
    blink::WebSwapResult,
    base::TimeTicks) {
  GetAssociatedFrameWidgetHost()->DidFirstVisuallyNonEmptyPaint();
}

void WebFrameWidgetBase::RequestAnimationAfterDelay(
    const base::TimeDelta& delay) {
  DCHECK(request_animation_after_delay_timer_.get());
  if (request_animation_after_delay_timer_->IsActive() &&
      request_animation_after_delay_timer_->NextFireInterval() > delay) {
    request_animation_after_delay_timer_->Stop();
  }
  if (!request_animation_after_delay_timer_->IsActive()) {
    request_animation_after_delay_timer_->StartOneShot(delay, FROM_HERE);
  }
}

void WebFrameWidgetBase::RequestAnimationAfterDelayTimerFired(TimerBase*) {
  if (client_)
    client_->ScheduleAnimation();
}

base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
WebFrameWidgetBase::EnsureCompositorMutatorDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) {
  if (!mutator_task_runner_) {
    widget_base_->LayerTreeHost()->SetLayerTreeMutator(
        AnimationWorkletMutatorDispatcherImpl::CreateCompositorThreadClient(
            &mutator_dispatcher_, &mutator_task_runner_));
  }

  DCHECK(mutator_task_runner_);
  *mutator_task_runner = mutator_task_runner_;
  return mutator_dispatcher_;
}

cc::AnimationHost* WebFrameWidgetBase::AnimationHost() const {
  return widget_base_->AnimationHost();
}

base::WeakPtr<PaintWorkletPaintDispatcher>
WebFrameWidgetBase::EnsureCompositorPaintDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* paint_task_runner) {
  // We check paint_task_runner_ not paint_dispatcher_ because the dispatcher is
  // a base::WeakPtr that should only be used on the compositor thread.
  if (!paint_task_runner_) {
    widget_base_->LayerTreeHost()->SetPaintWorkletLayerPainter(
        PaintWorkletPaintDispatcher::CreateCompositorThreadPainter(
            &paint_dispatcher_));
    paint_task_runner_ = Thread::CompositorThread()->GetTaskRunner();
  }
  DCHECK(paint_task_runner_);
  *paint_task_runner = paint_task_runner_;
  return paint_dispatcher_;
}

void WebFrameWidgetBase::SetDelegatedInkMetadata(
    std::unique_ptr<viz::DelegatedInkMetadata> metadata) {
  widget_base_->LayerTreeHost()->SetDelegatedInkMetadata(std::move(metadata));
}

// Enables measuring and reporting both presentation times and swap times in
// swap promises.
class ReportTimeSwapPromise : public cc::SwapPromise {
 public:
  ReportTimeSwapPromise(WebReportTimeCallback swap_time_callback,
                        WebReportTimeCallback presentation_time_callback,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        WebFrameWidgetBase* widget)
      : swap_time_callback_(std::move(swap_time_callback)),
        presentation_time_callback_(std::move(presentation_time_callback)),
        task_runner_(std::move(task_runner)),
        widget_(widget) {}
  ~ReportTimeSwapPromise() override = default;

  void DidActivate() override {}

  void WillSwap(viz::CompositorFrameMetadata* metadata) override {
    DCHECK_GT(metadata->frame_token, 0u);
    // The interval between the current swap and its presentation time is
    // reported in UMA (see corresponding code in DidSwap() below).
    frame_token_ = metadata->frame_token;
  }

  void DidSwap() override {
    DCHECK_GT(frame_token_, 0u);
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &RunCallbackAfterSwap, widget_, base::TimeTicks::Now(),
            std::move(swap_time_callback_),
            std::move(presentation_time_callback_), frame_token_));
  }

  cc::SwapPromise::DidNotSwapAction DidNotSwap(
      DidNotSwapReason reason) override {
    WebSwapResult result;
    switch (reason) {
      case cc::SwapPromise::DidNotSwapReason::SWAP_FAILS:
        result = WebSwapResult::kDidNotSwapSwapFails;
        break;
      case cc::SwapPromise::DidNotSwapReason::COMMIT_FAILS:
        result = WebSwapResult::kDidNotSwapCommitFails;
        break;
      case cc::SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE:
        result = WebSwapResult::kDidNotSwapCommitNoUpdate;
        break;
      case cc::SwapPromise::DidNotSwapReason::ACTIVATION_FAILS:
        result = WebSwapResult::kDidNotSwapActivationFails;
        break;
    }
    // During a failed swap, return the current time regardless of whether we're
    // using presentation or swap timestamps.
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            [](WebSwapResult result, base::TimeTicks swap_time,
               WebReportTimeCallback swap_time_callback,
               WebReportTimeCallback presentation_time_callback) {
              ReportTime(std::move(swap_time_callback), result, swap_time);
              ReportTime(std::move(presentation_time_callback), result,
                         swap_time);
            },
            result, base::TimeTicks::Now(), std::move(swap_time_callback_),
            std::move(presentation_time_callback_)));
    return DidNotSwapAction::BREAK_PROMISE;
  }

  int64_t TraceId() const override { return 0; }

 private:
  static void RunCallbackAfterSwap(
      WebFrameWidgetBase* widget,
      base::TimeTicks swap_time,
      WebReportTimeCallback swap_time_callback,
      WebReportTimeCallback presentation_time_callback,
      int frame_token) {
    // If the widget was collected or the widget wasn't collected yet, but
    // it was closed don't schedule a presentation callback.
    if (widget && widget->widget_base_) {
      widget->widget_base_->AddPresentationCallback(
          frame_token,
          WTF::Bind(&RunCallbackAfterPresentation,
                    std::move(presentation_time_callback), swap_time));
      ReportTime(std::move(swap_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
    } else {
      ReportTime(std::move(swap_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
      ReportTime(std::move(presentation_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
    }
  }

  static void RunCallbackAfterPresentation(
      WebReportTimeCallback presentation_time_callback,
      base::TimeTicks swap_time,
      base::TimeTicks presentation_time) {
    DCHECK(!swap_time.is_null());
    bool presentation_time_is_valid =
        !presentation_time.is_null() && (presentation_time > swap_time);
    UMA_HISTOGRAM_BOOLEAN("PageLoad.Internal.Renderer.PresentationTime.Valid",
                          presentation_time_is_valid);
    if (presentation_time_is_valid) {
      // This measures from 1ms to 10seconds.
      UMA_HISTOGRAM_TIMES(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime",
          presentation_time - swap_time);
    }
    ReportTime(std::move(presentation_time_callback), WebSwapResult::kDidSwap,
               presentation_time_is_valid ? presentation_time : swap_time);
  }

  static void ReportTime(WebReportTimeCallback callback,
                         WebSwapResult result,
                         base::TimeTicks time) {
    if (callback)
      std::move(callback).Run(result, time);
  }

  WebReportTimeCallback swap_time_callback_;
  WebReportTimeCallback presentation_time_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  CrossThreadWeakPersistent<WebFrameWidgetBase> widget_;
  uint32_t frame_token_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReportTimeSwapPromise);
};

void WebFrameWidgetBase::NotifySwapAndPresentationTimeInBlink(
    WebReportTimeCallback swap_time_callback,
    WebReportTimeCallback presentation_time_callback) {
  NotifySwapAndPresentationTime(std::move(swap_time_callback),
                                std::move(presentation_time_callback));
}

void WebFrameWidgetBase::NotifySwapAndPresentationTime(
    WebReportTimeCallback swap_time_callback,
    WebReportTimeCallback presentation_time_callback) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->QueueSwapPromise(
      std::make_unique<ReportTimeSwapPromise>(
          std::move(swap_time_callback), std::move(presentation_time_callback),
          widget_base_->LayerTreeHost()
              ->GetTaskRunnerProvider()
              ->MainThreadTaskRunner(),
          this));
}

scheduler::WebRenderWidgetSchedulingState*
WebFrameWidgetBase::RendererWidgetSchedulingState() {
  return widget_base_->RendererWidgetSchedulingState();
}

void WebFrameWidgetBase::WaitForDebuggerWhenShown() {
  local_root_->WaitForDebuggerWhenShown();
}

void WebFrameWidgetBase::SetTextZoomFactor(float text_zoom_factor) {
  local_root_->GetFrame()->SetTextZoomFactor(text_zoom_factor);
}

float WebFrameWidgetBase::TextZoomFactor() {
  return local_root_->GetFrame()->TextZoomFactor();
}

void WebFrameWidgetBase::SetMainFrameOverlayColor(SkColor color) {
  DCHECK(!local_root_->Parent());
  local_root_->GetFrame()->SetMainFrameColorOverlay(color);
}

void WebFrameWidgetBase::AddEditCommandForNextKeyEvent(const WebString& name,
                                                       const WebString& value) {
  edit_commands_.push_back(mojom::blink::EditCommand::New(name, value));
}

bool WebFrameWidgetBase::HandleCurrentKeyboardEvent() {
  bool did_execute_command = false;
  WebLocalFrame* frame = FocusedWebLocalFrameInWidget();
  if (!frame)
    frame = local_root_;
  for (const auto& command : edit_commands_) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame->ExecuteCommand(command->name, command->value))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

void WebFrameWidgetBase::ClearEditCommands() {
  edit_commands_ = Vector<mojom::blink::EditCommandPtr>();
}

WebTextInputInfo WebFrameWidgetBase::TextInputInfo() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return WebTextInputInfo();
  return controller->TextInputInfo();
}

ui::mojom::blink::VirtualKeyboardVisibilityRequest
WebFrameWidgetBase::GetLastVirtualKeyboardVisibilityRequest() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return ui::mojom::blink::VirtualKeyboardVisibilityRequest::NONE;
  return controller->GetLastVirtualKeyboardVisibilityRequest();
}

bool WebFrameWidgetBase::ShouldSuppressKeyboardForFocusedElement() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return false;
  return focused_frame->ShouldSuppressKeyboardForFocusedElement();
}

void WebFrameWidgetBase::GetEditContextBoundsInWindow(
    base::Optional<gfx::Rect>* edit_context_control_bounds,
    base::Optional<gfx::Rect>* edit_context_selection_bounds) {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return;
  WebRect control_bounds;
  WebRect selection_bounds;
  controller->GetLayoutBounds(&control_bounds, &selection_bounds);
  client_->ConvertViewportToWindow(&control_bounds);
  edit_context_control_bounds->emplace(control_bounds);
  if (controller->IsEditContextActive()) {
    client_->ConvertViewportToWindow(&selection_bounds);
    edit_context_selection_bounds->emplace(selection_bounds);
  }
}

int32_t WebFrameWidgetBase::ComputeWebTextInputNextPreviousFlags() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return 0;
  return controller->ComputeWebTextInputNextPreviousFlags();
}

void WebFrameWidgetBase::ResetVirtualKeyboardVisibilityRequest() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return;
  controller->SetVirtualKeyboardVisibilityRequest(
      ui::mojom::blink::VirtualKeyboardVisibilityRequest::NONE);
  ;
}

bool WebFrameWidgetBase::GetSelectionBoundsInWindow(
    gfx::Rect* focus,
    gfx::Rect* anchor,
    base::i18n::TextDirection* focus_dir,
    base::i18n::TextDirection* anchor_dir,
    bool* is_anchor_first) {
  if (Client()->ShouldDispatchImeEventsToPepper()) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    gfx::Rect pepper_caret = Client()->GetPepperCaretBounds();
    if (pepper_caret == *focus && pepper_caret == *anchor)
      return false;
    *focus = pepper_caret;
    *anchor = *focus;
    return true;
  }
  WebRect focus_webrect;
  WebRect anchor_webrect;
  SelectionBounds(focus_webrect, anchor_webrect);
  client_->ConvertViewportToWindow(&focus_webrect);
  client_->ConvertViewportToWindow(&anchor_webrect);

  // if the bounds are the same return false.
  if (gfx::Rect(focus_webrect) == *focus &&
      gfx::Rect(anchor_webrect) == *anchor)
    return false;
  *focus = gfx::Rect(focus_webrect);
  *anchor = gfx::Rect(anchor_webrect);

  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return true;
  focused_frame->SelectionTextDirection(*focus_dir, *anchor_dir);
  *is_anchor_first = focused_frame->IsSelectionAnchorFirst();
  return true;
}

void WebFrameWidgetBase::ClearTextInputState() {
  widget_base_->ClearTextInputState();
}

void WebFrameWidgetBase::SetFocus(bool focus) {
  widget_base_->SetFocus(focus);
}

bool WebFrameWidgetBase::HasFocus() {
  return widget_base_->has_focus();
}

void WebFrameWidgetBase::SetToolTipText(const String& tooltip_text,
                                        TextDirection dir) {
  widget_base_->SetToolTipText(tooltip_text, dir);
}

void WebFrameWidgetBase::DidOverscroll(
    const gfx::Vector2dF& overscroll_delta,
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::PointF& position,
    const gfx::Vector2dF& velocity) {
#if defined(OS_MACOSX)
  // On OSX the user can disable the elastic overscroll effect. If that's the
  // case, don't forward the overscroll notification.
  if (!widget_base_->LayerTreeHost()->GetSettings().enable_elastic_overscroll)
    return;
#endif

  cc::OverscrollBehavior overscroll_behavior =
      widget_base_->LayerTreeHost()->overscroll_behavior();
  if (!widget_base_->input_handler().DidOverscrollFromBlink(
          overscroll_delta, accumulated_overscroll, position, velocity,
          overscroll_behavior))
    return;

  // If we're currently handling an event, stash the overscroll data such that
  // it can be bundled in the event ack.
  Client()->DidOverscroll(overscroll_delta, accumulated_overscroll, position,
                          velocity, overscroll_behavior);
}

void WebFrameWidgetBase::InjectGestureScrollEvent(
    blink::WebGestureDevice device,
    const gfx::Vector2dF& delta,
    ui::ScrollGranularity granularity,
    cc::ElementId scrollable_area_element_id,
    blink::WebInputEvent::Type injected_type) {
  widget_base_->input_handler().InjectGestureScrollEvent(
      device, delta, granularity, scrollable_area_element_id, injected_type);
}

void WebFrameWidgetBase::DidChangeCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
  Client()->DidChangeCursor(cursor);
}

void WebFrameWidgetBase::FocusChangeComplete() {
  blink::WebLocalFrame* focused = LocalRoot()->View()->FocusedFrame();

  if (focused && focused->AutofillClient())
    focused->AutofillClient()->DidCompleteFocusChangeInFrame();
}

void WebFrameWidgetBase::ShowVirtualKeyboardOnElementFocus() {
  widget_base_->ShowVirtualKeyboardOnElementFocus();
}

void WebFrameWidgetBase::ProcessTouchAction(WebTouchAction touch_action) {
  if (!widget_base_->ProcessTouchAction(touch_action))
    return;
  Client()->SetTouchAction(touch_action);
}

void WebFrameWidgetBase::DidHandleGestureEvent(const WebGestureEvent& event,
                                               bool event_cancelled) {
  if (event_cancelled) {
    // The delegate() doesn't need to hear about cancelled events.
    return;
  }

#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event.GetType() == WebInputEvent::Type::kGestureTap) {
    widget_base_->input_handler().ShowVirtualKeyboard();
  } else if (event.GetType() == WebInputEvent::Type::kGestureLongPress) {
    WebInputMethodController* controller = GetActiveWebInputMethodController();
    if (!controller || controller->TextInputInfo().value.IsEmpty())
      widget_base_->input_handler().UpdateTextInputState();
    else
      widget_base_->input_handler().ShowVirtualKeyboard();
  }
#endif
}

gfx::Range WebFrameWidgetBase::CompositionRange() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return gfx::Range::InvalidRange();
  blink::WebInputMethodController* controller =
      focused_frame->GetInputMethodController();
  WebRange web_range = controller->CompositionRange();
  if (web_range.IsNull())
    return gfx::Range::InvalidRange();
  return gfx::Range(web_range.StartOffset(), web_range.EndOffset());
}

void WebFrameWidgetBase::GetCompositionCharacterBoundsInWindow(
    Vector<gfx::Rect>* bounds) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  blink::WebInputMethodController* controller =
      focused_frame->GetInputMethodController();
  blink::WebVector<blink::WebRect> bounds_from_blink;
  if (!controller->GetCompositionCharacterBounds(bounds_from_blink))
    return;

  for (size_t i = 0; i < bounds_from_blink.size(); ++i) {
    Client()->ConvertViewportToWindow(&bounds_from_blink[i]);
    bounds->push_back(bounds_from_blink[i]);
  }
}

}  // namespace blink
