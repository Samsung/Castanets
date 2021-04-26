// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_observer.h"

#include "base/metrics/histogram_macros_local.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "chrome/browser/lite_video/lite_video_decider.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/browser/lite_video/lite_video_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"

namespace {

// Returns the LiteVideoDecider when the LiteVideo features is enabled.
lite_video::LiteVideoDecider* GetLiteVideoDeciderFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(lite_video::features::IsLiteVideoEnabled());
  if (!web_contents)
    return nullptr;

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    return LiteVideoKeyedServiceFactory::GetForProfile(profile)
        ->lite_video_decider();
  }
  return nullptr;
}

}  // namespace

// static
void LiteVideoObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (IsLiteVideoAllowedForUser(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    LiteVideoObserver::CreateForWebContents(web_contents);
  }
}

LiteVideoObserver::LiteVideoObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  lite_video_decider_ = GetLiteVideoDeciderFromWebContents(web_contents);
}

LiteVideoObserver::~LiteVideoObserver() {
  FlushUKMMetrics();
}

void LiteVideoObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);

  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!lite_video_decider_)
    return;

  lite_video::LiteVideoBlocklistReason blocklist_reason =
      lite_video::LiteVideoBlocklistReason::kUnknown;
  base::Optional<lite_video::LiteVideoHint> hint =
      lite_video_decider_->CanApplyLiteVideo(navigation_handle,
                                             &blocklist_reason);

  MaybeUpdateCoinflipExperimentState(navigation_handle);

  lite_video::LiteVideoDecision decision =
      MakeLiteVideoDecision(navigation_handle, hint);

  if (navigation_handle->IsInMainFrame()) {
    FlushUKMMetrics();
    nav_metrics_ = lite_video::LiteVideoNavigationMetrics(
        navigation_handle->GetNavigationId(), decision, blocklist_reason,
        lite_video::LiteVideoThrottleResult::kThrottledWithoutStop);
  }

  LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.Navigation.HasHint", hint ? true : false);

  if (decision == lite_video::LiteVideoDecision::kNotAllowed)
    return;

  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  if (!render_frame_host || !render_frame_host->GetProcess())
    return;

  mojo::AssociatedRemote<blink::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  auto hint_ptr = blink::mojom::LiteVideoHint::New();
  hint_ptr->target_downlink_bandwidth_kbps =
      hint->target_downlink_bandwidth_kbps();
  hint_ptr->kilobytes_to_buffer_before_throttle =
      hint->kilobytes_to_buffer_before_throttle();
  hint_ptr->target_downlink_rtt_latency = hint->target_downlink_rtt_latency();
  hint_ptr->max_throttling_delay = hint->max_throttling_delay();

  if (render_frame_host->GetRemoteAssociatedInterfaces()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &loading_hints_agent);
    loading_hints_agent->SetLiteVideoHint(std::move(hint_ptr));
  }
}

lite_video::LiteVideoDecision LiteVideoObserver::MakeLiteVideoDecision(
    content::NavigationHandle* navigation_handle,
    base::Optional<lite_video::LiteVideoHint> hint) const {
  if (hint) {
    return is_coinflip_holdback_ ? lite_video::LiteVideoDecision::kHoldback
                                 : lite_video::LiteVideoDecision::kAllowed;
  }
  return lite_video::LiteVideoDecision::kNotAllowed;
}

void LiteVideoObserver::FlushUKMMetrics() {
  if (!nav_metrics_)
    return;
  ukm::SourceId ukm_source_id = ukm::ConvertToSourceId(
      nav_metrics_->nav_id(), ukm::SourceIdType::NAVIGATION_ID);
  ukm::builders::LiteVideo builder(ukm_source_id);
  builder.SetThrottlingStartDecision(static_cast<int>(nav_metrics_->decision()))
      .SetBlocklistReason(static_cast<int>(nav_metrics_->blocklist_reason()))
      .SetThrottlingResult(static_cast<int>(nav_metrics_->throttle_result()))
      .Record(ukm::UkmRecorder::Get());
  nav_metrics_.reset();
}

// Returns the result of a coinflip.
void LiteVideoObserver::MaybeUpdateCoinflipExperimentState(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  if (!lite_video::features::IsCoinflipExperimentEnabled())
    return;

  is_coinflip_holdback_ = lite_video::switches::ShouldForceCoinflipHoldback()
                              ? true
                              : base::RandInt(0, 1);
}

void LiteVideoObserver::MediaBufferUnderflow(const content::MediaPlayerId& id) {
  content::RenderFrameHost* render_frame_host = id.render_frame_host;

  if (!render_frame_host || !render_frame_host->GetProcess())
    return;

  mojo::AssociatedRemote<blink::mojom::PreviewsResourceLoadingHintsReceiver>
      loading_hints_agent;

  if (render_frame_host->GetRemoteAssociatedInterfaces()) {
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &loading_hints_agent);
    loading_hints_agent->StopThrottlingMediaRequests();
  }
  // Only consider a rebuffer event related to LiteVideos if they
  // were allowed on current navigation.
  if (!nav_metrics_ ||
      nav_metrics_->decision() != lite_video::LiteVideoDecision::kAllowed) {
    return;
  }

  nav_metrics_->SetThrottleResult(
      lite_video::LiteVideoThrottleResult::kThrottleStoppedOnRebuffer);

  if (!lite_video_decider_)
    return;

  // Determine if the rebuffer happened in the mainframe.
  render_frame_host->GetMainFrame() == render_frame_host
      ? lite_video_decider_->DidMediaRebuffer(
            render_frame_host->GetLastCommittedURL(), base::nullopt, true)
      : lite_video_decider_->DidMediaRebuffer(
            render_frame_host->GetMainFrame()->GetLastCommittedURL(),
            render_frame_host->GetLastCommittedURL(), true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LiteVideoObserver)
