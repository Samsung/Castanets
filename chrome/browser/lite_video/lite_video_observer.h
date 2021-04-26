// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace lite_video {
class LiteVideoDecider;
class LiteVideoHint;
}  // namespace lite_video

class LiteVideoObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LiteVideoObserver> {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~LiteVideoObserver() override;

 private:
  friend class content::WebContentsUserData<LiteVideoObserver>;
  explicit LiteVideoObserver(content::WebContents* web_contents);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaBufferUnderflow(const content::MediaPlayerId& id) override;

  // Determines the LiteVideoDecision based on |hint| and the coinflip
  // holdback state.
  lite_video::LiteVideoDecision MakeLiteVideoDecision(
      content::NavigationHandle* navigation_handle,
      base::Optional<lite_video::LiteVideoHint> hint) const;

  // Records the metrics for LiteVideos applied to any frames associated with
  // the current mainframe navigation id. Called once per mainframe.
  void FlushUKMMetrics();

  // Updates the coinflip state if the navigation handle is associated with
  // the mainframe. Should only be called once per new mainframe navigation.
  void MaybeUpdateCoinflipExperimentState(
      content::NavigationHandle* navigation_handle);

  // The decider capable of making decisions about whether LiteVideos should be
  // applied and the params to use when throttling media requests.
  lite_video::LiteVideoDecider* lite_video_decider_ = nullptr;

  // The current metrics about the navigation |this| is observing. Reset
  // after each time the metrics being held are recorded as a UKM event.
  base::Optional<lite_video::LiteVideoNavigationMetrics> nav_metrics_;

  // Whether the navigations currently being observed should have the LiteVideo
  // optimization heldback due to a coinflip, counterfactual experiment.
  // |is_coinflip_holdback_| is updated each time a mainframe navigation
  // commits.
  bool is_coinflip_holdback_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_OBSERVER_H_
