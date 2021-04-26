// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_METADATA_H_
#define MEDIA_BASE_VIDEO_FRAME_METADATA_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/video_transformation.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class MEDIA_EXPORT VideoFrameMetadata {
 public:
  enum Key {
    ALLOW_OVERLAY,
    CAPTURE_BEGIN_TIME,
    CAPTURE_END_TIME,
    CAPTURE_COUNTER,
    CAPTURE_UPDATE_RECT,
    COPY_REQUIRED,
    END_OF_STREAM,
    FRAME_DURATION,
    FRAME_RATE,
    INTERACTIVE_CONTENT,
    REFERENCE_TIME,
    RESOURCE_UTILIZATION,
    READ_LOCK_FENCES_ENABLED,
    ROTATION,
    TEXTURE_OWNER,
    WANTS_PROMOTION_HINT,
    PROTECTED_VIDEO,
    HW_PROTECTED,
    OVERLAY_PLANE_ID,
    POWER_EFFICIENT,
    DEVICE_SCALE_FACTOR,
    PAGE_SCALE_FACTOR,
    ROOT_SCROLL_OFFSET_X,
    ROOT_SCROLL_OFFSET_Y,
    TOP_CONTROLS_VISIBLE_HEIGHT,
    DECODE_BEGIN_TIME,
    DECODE_END_TIME,
    PROCESSING_TIME,
    RTP_TIMESTAMP,
    RECEIVE_TIME,
    WALLCLOCK_FRAME_DURATION,

    NUM_KEYS
  };

  VideoFrameMetadata();
  ~VideoFrameMetadata();

  VideoFrameMetadata(const VideoFrameMetadata& other);

  // Merges internal values from |metadata_source|.
  void MergeMetadataFrom(const VideoFrameMetadata* metadata_source);

  // Sources of VideoFrames use this marker to indicate that the associated
  // VideoFrame can be overlaid, case in which its contents do not need to be
  // further composited but displayed directly.
  bool allow_overlay = false;

  // Video capture begin/end timestamps.  Consumers can use these values for
  // dynamic optimizations, logging stats, etc.
  base::Optional<base::TimeTicks> capture_begin_time;
  base::Optional<base::TimeTicks> capture_end_time;

  // A counter that is increased by the producer of video frames each time
  // it pushes out a new frame. By looking for gaps in this counter, clients
  // can determine whether or not any frames have been dropped on the way from
  // the producer between two consecutively received frames. Note that the
  // counter may start at arbitrary values, so the absolute value of it has no
  // meaning.
  base::Optional<int> capture_counter;

  // The rectangular region of the frame that has changed since the frame
  // with the directly preceding CAPTURE_COUNTER. If that frame was not
  // received, typically because it was dropped during transport from the
  // producer, clients must assume that the entire frame has changed.
  // The rectangle is relative to the full frame data, i.e. [0, 0,
  // coded_size().width(), coded_size().height()]. It does not have to be
  // fully contained within visible_rect().
  base::Optional<gfx::Rect> capture_update_rect;

  // Indicates that this frame must be copied to a new texture before use,
  // rather than being used directly. Specifically this is required for
  // WebView because of limitations about sharing surface textures between GL
  // contexts.
  bool copy_required = false;

  // Indicates if the current frame is the End of its current Stream.
  bool end_of_stream = false;

  // The estimated duration of this frame (i.e., the amount of time between
  // the media timestamp of this frame and the next).  Note that this is not
  // the same information provided by FRAME_RATE as the FRAME_DURATION can
  // vary unpredictably for every frame.  Consumers can use this to optimize
  // playback scheduling, make encoding quality decisions, and/or compute
  // frame-level resource utilization stats.
  base::Optional<base::TimeDelta> frame_duration;

  // Represents either the fixed frame rate, or the maximum frame rate to
  // expect from a variable-rate source.  This value generally remains the
  // same for all frames in the same session.
  base::Optional<double> frame_rate;

  // This is a boolean that signals that the video capture engine detects
  // interactive content. One possible optimization that this signal can help
  // with is remote content: adjusting end-to-end latency down to help the
  // user better coordinate their actions.
  bool interactive_content = false;

  // This field represents the local time at which either: 1) the frame was
  // generated, if it was done so locally; or 2) the targeted play-out time
  // of the frame, if it was generated from a remote source. This value is NOT
  // a high-resolution timestamp, and so it should not be used as a
  // presentation time; but, instead, it should be used for buffering playback
  // and for A/V synchronization purposes.
  base::Optional<base::TimeTicks> reference_time;

  // A feedback signal that indicates the fraction of the tolerable maximum
  // amount of resources that were utilized to process this frame.  A producer
  // can check this value after-the-fact, usually via a VideoFrame destruction
  // observer, to determine whether the consumer can handle more or less data
  // volume, and achieve the right quality versus performance trade-off.
  //
  // Values are interpreted as follows:
  // Less than 0.0 is meaningless and should be ignored.  1.0 indicates a
  // maximum sustainable utilization.  Greater than 1.0 indicates the consumer
  // is likely to stall or drop frames if the data volume is not reduced.
  //
  // Example: In a system that encodes and transmits video frames over the
  // network, this value can be used to indicate whether sufficient CPU
  // is available for encoding and/or sufficient bandwidth is available for
  // transmission over the network.  The maximum of the two utilization
  // measurements would be used as feedback.
  base::Optional<double> resource_utilization;

  // Sources of VideoFrames use this marker to indicate that an instance of
  // VideoFrameExternalResources produced from the associated video frame
  // should use read lock fences.
  bool read_lock_fences_enabled = false;

  // Indicates that the frame is rotated.
  base::Optional<VideoRotation> rotation;

  // Android only: if set, then this frame is not suitable for overlay, even
  // if ALLOW_OVERLAY is set.  However, it allows us to process the overlay
  // to see if it would have been promoted, if it were backed by a SurfaceView
  // instead.  This lets us figure out when SurfaceViews are appropriate.
  bool texture_owner = false;

  // Android only: if set, then this frame's resource would like to be
  // notified about its promotability to an overlay.
  bool wants_promotion_hint = false;

  // This video frame comes from protected content.
  bool protected_video = false;

  // This video frame is protected by hardware. This option is valid only if
  // PROTECTED_VIDEO is also set to true.
  bool hw_protected = false;

  // An UnguessableToken that identifies VideoOverlayFactory that created
  // this VideoFrame. It's used by Cast to help with video hole punch.
  base::Optional<base::UnguessableToken> overlay_plane_id;

  // Whether this frame was decoded in a power efficient way.
  bool power_efficient = false;

  // CompositorFrameMetadata variables associated with this frame. Used for
  // remote debugging.
  // TODO(crbug.com/832220): Use a customized dictionary value instead of
  // using these keys directly.
  base::Optional<double> device_scale_factor;
  base::Optional<double> page_scale_factor;
  base::Optional<double> root_scroll_offset_x;
  base::Optional<double> root_scroll_offset_y;
  base::Optional<double> top_controls_visible_height;

  // If present, this field represents the local time at which the VideoFrame
  // was decoded from whichever format it was encoded in. Sometimes only
  // DECODE_END_TIME will be present.
  base::Optional<base::TimeTicks> decode_begin_time;
  base::Optional<base::TimeTicks> decode_end_time;

  // If present, this field represents the elapsed time from the submission of
  // the encoded packet with the same PTS as this frame to the decoder until
  // the decoded frame was ready for presentation.
  base::Optional<base::TimeDelta> processing_time;

  // The RTP timestamp associated with this video frame. Stored as a double
  // since base::DictionaryValue doesn't have a uint32_t type.
  //
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource
  base::Optional<double> rtp_timestamp;

  // For video frames coming from a remote source, this is the time the
  // encoded frame was received by the platform, i.e., the time at
  // which the last packet belonging to this frame was received over the
  // network.
  base::Optional<base::TimeTicks> receive_time;

  // If present, this field represents the duration this frame is ideally
  // expected to spend on the screen during playback. Unlike FRAME_DURATION
  // this field takes into account current playback rate.
  base::Optional<base::TimeDelta> wallclock_frame_duration;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_METADATA_H_
