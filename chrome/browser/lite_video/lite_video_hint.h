// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_H_

#include <stdint.h>

#include "base/time/time.h"

namespace lite_video {

class LiteVideoHint {
 public:
  LiteVideoHint(int target_downlink_bandwidth_kbps,
                base::TimeDelta target_downlink_rtt_latency,
                int kilobytes_to_buffer_before_throttle,
                base::TimeDelta max_throttling_delay);
  ~LiteVideoHint() = default;

  int target_downlink_bandwidth_kbps() const {
    return target_downlink_bandwidth_kbps_;
  }

  base::TimeDelta target_downlink_rtt_latency() const {
    return target_downlink_rtt_latency_;
  }

  int kilobytes_to_buffer_before_throttle() const {
    return kilobytes_to_buffer_before_throttle_;
  }

  base::TimeDelta max_throttling_delay() const { return max_throttling_delay_; }

 private:
  const int target_downlink_bandwidth_kbps_;
  const base::TimeDelta target_downlink_rtt_latency_;
  const int kilobytes_to_buffer_before_throttle_;
  const base::TimeDelta max_throttling_delay_;
};

}  // namespace lite_video

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_HINT_H_
