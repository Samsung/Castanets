// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CASTANETS_CASTANETS_WEBMEDIAPLAYER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_CASTANETS_CASTANETS_WEBMEDIAPLAYER_IMPL_H_

#include "media/blink/renderer_media_player_interface.h"
#include "media/blink/video_frame_compositor.h"
#include "media/renderers/paint_canvas_video_renderer.h"

namespace blink {
class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}  // namespace blink

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cc {
class VideoLayer;
}

namespace media {
class MediaLog;
class RendererMediaPlayerManagerInterface;
class UrlIndex;
class WebAudioSourceProviderImpl;
class WebMediaPlayerDelegate;
}  // namespace media

namespace content {
class RendererMediaPlayerManager;

// This class implements blink::WebMediaPlayer by keeping the castanets
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class WebMediaPlayerCastanets
    : public blink::WebMediaPlayer,
      public media::RendererMediaPlayerInterface,
      public media::WebMediaPlayerDelegate::Observer,
      public base::SupportsWeakPtr<WebMediaPlayerCastanets> {
 public:
  // Construct a WebMediaPlayerCastanets object. This class communicates
  // with the WebMediaPlayerCastanets object in the browser process through
  // |proxy|.
  WebMediaPlayerCastanets(
      blink::WebLocalFrame* frame,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      media::WebMediaPlayerDelegate* delegate,
      media::UrlIndex* url_index,
      std::unique_ptr<media::VideoFrameCompositor> compositor,
      std::unique_ptr<media::WebMediaPlayerParams> params);
  ~WebMediaPlayerCastanets() override;

  LoadTiming Load(LoadType,
                  const blink::WebMediaPlayerSource&,
                  CORSMode) override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  void Seek(double seconds) override;
  void SetRate(double rate) override;
  void SetVolume(double volume) override;

  // Enter Picture-in-Picture and notifies Blink with window size
  // when video successfully enters Picture-in-Picture.
  void EnterPictureInPicture(PipWindowOpenedCallback) override;

  // Exit Picture-in-Picture and notifies Blink when it's done.
  void ExitPictureInPicture(PipWindowClosedCallback) override;

  // Register a callback that will be run when the Picture-in-Picture window
  // is resized.
  void RegisterPictureInPictureWindowResizeCallback(
      PipWindowResizedCallback) override;

  blink::WebTimeRanges Buffered() const override;
  blink::WebTimeRanges Seekable() const override;

  // Attempts to switch the audio output device.
  // Implementations of SetSinkId take ownership of the WebSetSinkCallbacks
  // object.
  // Note also that SetSinkId implementations must make sure that all
  // methods of the WebSetSinkCallbacks object, including constructors and
  // destructors, run in the same thread where the object is created
  // (i.e., the blink thread).
  void SetSinkId(const blink::WebString& sink_id,
                 blink::WebSetSinkIdCallbacks*) override;

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  // Dimension of the video.
  blink::WebSize NaturalSize() const override;

  blink::WebSize VisibleRect() const override;

  // Getters of playback state.
  bool Paused() const override;
  bool Seeking() const override;
  double Duration() const override;
  double CurrentTime() const override;

  // Internal states of loading and network.
  NetworkState GetNetworkState() const override;
  ReadyState GetReadyState() const override;

  // Returns an implementation-specific human readable error message, or an
  // empty string if no message is available. The message should begin with a
  // UA-specific-error-code (without any ':'), optionally followed by ': ' and
  // further description of the error.
  blink::WebString GetErrorMessage() const override;

  bool DidLoadingProgress() override;

  bool DidGetOpaqueResponseFromServiceWorker() const override;
  bool HasSingleSecurityOrigin() const override;
  bool DidPassCORSAccessCheck() const override;

  double MediaTimeForTimeValue(double time_value) const override;

  unsigned DecodedFrameCount() const override;
  unsigned DroppedFrameCount() const override;
  size_t AudioDecodedByteCount() const override;
  size_t VideoDecodedByteCount() const override;

  bool CopyVideoTextureToPlatformTexture(
      gpu::gles2::GLES2Interface* gl,
      unsigned int target,
      unsigned int texture,
      unsigned internal_format,
      unsigned format,
      unsigned type,
      int level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      VideoFrameUploadMetadata* out_metadata) override;

  // |out_metadata|, if set, is used to return metadata about the frame that is
  // uploaded during this call. |already_uploaded_id| indicates the unique_id of
  // the frame last uploaded to this destination. It should only be set by the
  // caller if the contents of the destination are known not to have changed
  // since that upload. - If |out_metadata| is not null, |already_uploaded_id|
  // is compared with the unique_id of the frame being uploaded. If it's the
  // same, the upload may be skipped and considered to be successful.
  void Paint(cc::PaintCanvas* canvas,
             const blink::WebRect& rect,
             cc::PaintFlags& flags,
             int already_uploaded_id,
             VideoFrameUploadMetadata* out_metadata) override;

  void SetContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) override;

  void EnteredFullscreen() override;
  void ExitedFullscreen() override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnFrameHidden() override;
  void OnFrameClosed() override;
  void OnFrameShown() override;
  void OnIdleTimeout() override;
  void OnPlay() override;
  void OnPause() override;
  void OnSeekForward(double seconds) override;
  void OnSeekBackward(double seconds) override;
  void OnVolumeMultiplierUpdate(double multiplier) override;
  void OnBecamePersistentVideo(bool value) override;
  void OnPictureInPictureModeEnded() override;
  void OnPictureInPictureControlClicked(const std::string& control_id) override;

  // RendererMediaPlayerInterface implementation
  void OnMediaMetadataChanged(base::TimeDelta duration,
                              int width,
                              int height,
                              bool success) override {}
  void OnPlaybackComplete() override {}
  void OnSeekComplete(base::TimeDelta current_time) override {}
  void OnMediaError(int error_type) override {}
  void OnVideoSizeChanged(int width, int height) override {}
  void OnTimeUpdate(base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks) override {}
  void OnPlayerReleased() override {}
  void OnConnectedToRemoteDevice(
      const std::string& remote_playback_message) override {}
  void OnDisconnectedFromRemoteDevice() override {}
  void OnCancelledRemotePlaybackRequest() override {}
  void OnRemotePlaybackStarted() override {}
  void OnDidExitFullscreen() override {}
  void OnMediaPlayerPlay() override {}
  void OnMediaPlayerPause() override {}
  void OnRemoteRouteAvailabilityChanged(
      blink::WebRemotePlaybackAvailability availability) override {}

  void OnMediaDataChange(int, int, int) override;
  void OnDurationChange(base::TimeDelta) override;

  // Called after a seek request is complete. Current time can be different
  // from the requested seek time.
  void OnTimeChanged() override;
  void OnTimeUpdate(base::TimeDelta) override;
  void OnBufferingUpdate(int) override;
  void OnPauseStateChange(bool) override;

  void OnSeekRequest(base::TimeDelta time_to_seek) override;

  // Internal seeks can happen. So don't include time as argument.
  void OnSeekComplete() override;

  void OnPlayerSuspend(bool) override;
  void OnPlayerResumed(bool) override;
  void OnPlayerDestroyed() override;

  void SetReadyState(WebMediaPlayer::ReadyState) override;
  void SetNetworkState(WebMediaPlayer::NetworkState) override;

  void SuspendAndReleaseResources() override;

  void SetMediaPlayerManager(
      media::RendererMediaPlayerManagerInterface* media_player_manager);

  void RequestPause();
  void ReleaseMediaResource();
  void InitializeMediaResource();

  void CreateVideoHoleFrame();
  void OnDrawableContentRectChanged(gfx::Rect rect, bool is_video);

  void StartLayerBoundUpdateTimer();
  void StopLayerBoundUpdateTimer();
  void OnLayerBoundUpdateTimerFired();

 private:
  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type, const blink::WebURL& url);
  void PauseInternal(bool is_media_related_action);

  void OnNaturalSizeChanged(gfx::Size size);
  void OnOpacityChanged(bool opaque);

  // Returns the current video frame from |compositor_|. Blocks until the
  // compositor can return the frame.
  scoped_refptr<media::VideoFrame> GetCurrentFrameFromCompositor() const;

  // Called whenever there is new frame to be painted.
  void FrameReady(const scoped_refptr<media::VideoFrame>& frame);

  // Calculate the boundary rectangle of the media player (i.e. location and
  // size of the video frame).
  // Returns true if the geometry has been changed since the last call.
  bool UpdateBoundaryRectangle();
  const gfx::RectF GetBoundaryRectangle();

  // TODO: Fix the scope!
  void Resume();

  blink::WebLocalFrame* frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // Message loops for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Manager for managing this object and for delegating method calls on
  // Render Thread.
  media::RendererMediaPlayerManagerInterface* manager_;

  blink::WebMediaPlayerClient* client_;

  std::unique_ptr<media::MediaLog> media_log_;

  media::WebMediaPlayerDelegate* const delegate_;
  int delegate_id_;

  media::WebMediaPlayerParams::DeferLoadCB defer_load_cb_;

  // Video rendering members.
  // The |compositor_| runs on the compositor thread, or if
  // kEnableSurfaceLayerForVideo is enabled, the media thread. This task runner
  // posts tasks for the |compositor_| on the correct thread.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Deleted on |compositor_task_runner_|.
  std::unique_ptr<media::VideoFrameCompositor> compositor_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  scoped_refptr<cc::VideoLayer> video_layer_;

  MediaPlayerHostMsg_Initialize_Type player_type_;

  // Player ID assigned by the |manager_|.
  int player_id_;

  int video_width_;
  int video_height_;

  bool audio_;
  bool video_;

  base::TimeDelta current_time_;
  base::TimeDelta duration_;
  bool is_paused_;

  bool is_seeking_;
  base::TimeDelta seek_time_;
  bool pending_seek_;
  base::TimeDelta pending_seek_time_;

  // Whether the video is known to be opaque or not.
  bool opaque_;
  bool is_fullscreen_;

  bool is_draw_ready_;
  bool pending_play_;

  // A rectangle represents the geometry of video frame, when computed last
  // time.
  gfx::RectF last_computed_rect_;
  base::RepeatingTimer layer_bound_update_timer_;

  gfx::Size natural_size_;
  blink::WebTimeRanges buffered_;
  mutable bool did_loading_progress_;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate(). The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;

  base::WeakPtrFactory<WebMediaPlayerCastanets> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerCastanets);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CASTANETS_CASTANETS_WEBMEDIAPLAYER_IMPL_H_
