// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/castanets/castanets_webmediaplayer_impl.h"

#include "cc/layers/video_layer.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_content_type.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace {
using media::VideoFrame;

GURL GetCleanURL(std::string url) {
  // FIXME: Need to consider "app://" scheme.
  CHECK(url.compare(0, 6, "app://"));
  if (!url.compare(0, 7, "file://")) {
    int position = url.find("?");
    if (position != -1)
      url.erase(url.begin() + position, url.end());
  }
  GURL url_(url);
  return url_;
}

const base::TimeDelta kLayerBoundUpdateInterval =
    base::TimeDelta::FromMilliseconds(50);
}  // namespace

namespace content {
WebMediaPlayerCastanets::WebMediaPlayerCastanets(
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    media::WebMediaPlayerDelegate* delegate,
    media::UrlIndex* url_index,
    std::unique_ptr<media::VideoFrameCompositor> compositor,
    std::unique_ptr<media::WebMediaPlayerParams> params)
    : frame_(frame),
      network_state_(blink::WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(blink::WebMediaPlayer::kReadyStateHaveNothing),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      client_(client),
      media_log_(params->take_media_log()),
      delegate_(delegate),
      defer_load_cb_(params->defer_load_cb()),
      compositor_task_runner_(params->video_frame_compositor_task_runner()),
      compositor_(std::move(compositor)),
      player_type_(MEDIA_PLAYER_TYPE_NONE),
      video_width_(0),
      video_height_(0),
      audio_(false),
      video_(false),
      is_paused_(true),
      is_seeking_(false),
      pending_seek_(false),
      opaque_(false),
      is_fullscreen_(false),
      is_draw_ready_(false),
      pending_play_(false),
      natural_size_(0, 0),
      buffered_(static_cast<size_t>(1)),
      did_loading_progress_(false),
      volume_(1.0),
      weak_factory_(this) {
  if (delegate_)
    delegate_id_ = delegate_->AddObserver(this);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &media::VideoFrameCompositor::SetDrawableContentRectChangedCallback,
          base::Unretained(compositor_.get()),
          media::BindToCurrentLoop(
              base::Bind(&WebMediaPlayerCastanets::OnDrawableContentRectChanged,
                         AsWeakPtr()))));

  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerCastanets::~WebMediaPlayerCastanets() {
  if (manager_) {
    manager_->DestroyPlayer(player_id_);
    manager_->UnregisterMediaPlayer(player_id_);
  }

  compositor_->SetVideoFrameProviderClient(NULL);
  client_->SetCcLayer(NULL);

  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }

  compositor_task_runner_->DeleteSoon(FROM_HERE, std::move(compositor_));
}

void WebMediaPlayerCastanets::SetMediaPlayerManager(
    media::RendererMediaPlayerManagerInterface* media_player_manager) {
  manager_ = media_player_manager;
  player_id_ = manager_->RegisterMediaPlayer(this);
}

blink::WebMediaPlayer::LoadTiming WebMediaPlayerCastanets::Load(
    LoadType load_type,
    const blink::WebMediaPlayerSource& source,
    CORSMode /* cors_mode */) {
  // Only URL is supported.
  DCHECK(source.IsURL());
  blink::WebURL url = source.GetAsURL();

  bool is_deferred = false;
  if (!defer_load_cb_.is_null()) {
    is_deferred = defer_load_cb_.Run(base::Bind(
        &WebMediaPlayerCastanets::DoLoad, AsWeakPtr(), load_type, url));
  } else {
    DoLoad(load_type, url);
  }
  return is_deferred ? LoadTiming::kDeferred : LoadTiming::kImmediate;
}

void WebMediaPlayerCastanets::DoLoad(LoadType load_type,
                                     const blink::WebURL& url) {
  switch (load_type) {
    case kLoadTypeURL:
      player_type_ = MEDIA_PLAYER_TYPE_URL_WITH_VIDEO_HOLE;
      break;
    default:
      LOG(ERROR) << "Unsupported load type #" << load_type;
      return;
  }

  int demuxer_client_id = 0;
  blink::WebString content_mime_type =
      blink::WebString(client_->GetContentMIMEType());

  manager_->Initialize(player_id_, player_type_,
                       GetCleanURL(url.GetString().Utf8()),
                       content_mime_type.Utf8(), demuxer_client_id);
}

void WebMediaPlayerCastanets::Play() {
  LOG(INFO) << __FUNCTION__ << " [" << player_id_ << "]";

  if (HasVideo() && !is_draw_ready_) {
    pending_play_ = true;
    return;
  }
  pending_play_ = false;

  manager_->Start(player_id_);
  // Has to be updated from |MediaPlayerCastanets| but IPC causes delay.
  // There are cases were play - pause are fired successively and would fail.
  is_paused_ = false;
  if (delegate_)
    delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                       media::DurationToMediaContentType(duration_));
}

void WebMediaPlayerCastanets::PauseInternal(bool is_media_related_action) {
  LOG(INFO) << __FUNCTION__ << " [" << player_id_ << "]"
            << " media_related:" << is_media_related_action;

  pending_play_ = false;
  manager_->Pause(player_id_, is_media_related_action);

  // Has to be updated from |MediaPlayerCastanets| but IPC causes delay.
  // There are cases were play - pause are fired successively and would fail.
  is_paused_ = true;
  if (delegate_)
    delegate_->DidPause(delegate_id_);
}

void WebMediaPlayerCastanets::Pause() {
  PauseInternal(false);
}

void WebMediaPlayerCastanets::ReleaseMediaResource() {
  LOG(INFO) << __FUNCTION__ << " Player[" << player_id_ << "]";
  manager_->Suspend(player_id_);
}

void WebMediaPlayerCastanets::InitializeMediaResource() {
  LOG(INFO) << __FUNCTION__ << " Player[" << player_id_
            << "] suspend_time : " << current_time_;
  manager_->Resume(player_id_);
}

void WebMediaPlayerCastanets::RequestPause() {
  LOG(INFO) << __FUNCTION__ << " Player[" << player_id_ << "]";
  switch (network_state_) {
    // Pause the media player and inform Blink if the player is in a good
    // shape.
    case blink::WebMediaPlayer::kNetworkStateIdle:
    case blink::WebMediaPlayer::kNetworkStateLoading:
    case blink::WebMediaPlayer::kNetworkStateLoaded:
      PauseInternal(false);
      client_->RequestPause();
      break;
    // If a WebMediaPlayer instance has entered into other then above states,
    // the internal network state in HTMLMediaElement could be set to empty.
    default:
      break;
  }
}

void WebMediaPlayerCastanets::Seek(double seconds) {
  LOG(INFO) << __FUNCTION__ << " Player[" << player_id_ << "]"
            << " seconds :" << seconds;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::TimeDelta new_seek_time = base::TimeDelta::FromSecondsD(seconds);
  if (is_seeking_) {
    if (new_seek_time == seek_time_) {
      pending_seek_ = false;
      return;
    }

    pending_seek_ = true;
    pending_seek_time_ = new_seek_time;

    // Later, OnSeekComplete will trigger the pending seek.
    return;
  }

  is_seeking_ = true;
  seek_time_ = new_seek_time;
  manager_->Seek(player_id_, seek_time_);
}

void WebMediaPlayerCastanets::SetRate(double rate) {
  manager_->SetRate(player_id_, rate);
}

void WebMediaPlayerCastanets::SetVolume(double volume) {
  manager_->SetVolume(player_id_, volume);
}

blink::WebTimeRanges WebMediaPlayerCastanets::Buffered() const {
  return buffered_;
}

blink::WebTimeRanges WebMediaPlayerCastanets::Seekable() const {
  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata)
    return blink::WebTimeRanges();

  const blink::WebTimeRange seekable_range(0.0, Duration());
  return blink::WebTimeRanges(&seekable_range, 1);
}

void WebMediaPlayerCastanets::Paint(cc::PaintCanvas* canvas,
                                    const blink::WebRect& rect,
                                    cc::PaintFlags& flags,
                                    int already_uploaded_id,
                                    VideoFrameUploadMetadata* out_metadata) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::OnFrameHidden() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  SuspendAndReleaseResources();
}

void WebMediaPlayerCastanets::OnFrameClosed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::OnFrameShown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  Resume();
}

void WebMediaPlayerCastanets::OnIdleTimeout() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::OnPlay() {
  client_->RequestPlay();
}

void WebMediaPlayerCastanets::OnPause() {
  client_->RequestPause();
}

void WebMediaPlayerCastanets::OnSeekForward(double seconds) {
  DCHECK_GE(seconds, 0) << "Attempted to seek by a negative number of seconds";
  client_->RequestSeek(CurrentTime() + seconds);
}

void WebMediaPlayerCastanets::OnSeekBackward(double seconds) {
  DCHECK_GE(seconds, 0) << "Attempted to seek by a negative number of seconds";
  client_->RequestSeek(CurrentTime() - seconds);
}

void OnSeekRequest(base::TimeDelta time_to_seek) {}

void WebMediaPlayerCastanets::OnVolumeMultiplierUpdate(double multiplier) {
  SetVolume(volume_);
}

void WebMediaPlayerCastanets::OnBecamePersistentVideo(bool value) {
  client_->OnBecamePersistentVideo(value);
}

void WebMediaPlayerCastanets::OnPictureInPictureModeEnded() {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::OnPictureInPictureControlClicked(
    const std::string& control_id) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::EnterPictureInPicture(
    blink::WebMediaPlayer::PipWindowOpenedCallback callback) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::ExitPictureInPicture(
    blink::WebMediaPlayer::PipWindowClosedCallback callback) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::RegisterPictureInPictureWindowResizeCallback(
    blink::WebMediaPlayer::PipWindowResizedCallback callback) {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::SetSinkId(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCallbacks* web_callback) {
  NOTIMPLEMENTED();
}

bool WebMediaPlayerCastanets::HasVideo() const {
  return video_;
}

bool WebMediaPlayerCastanets::HasAudio() const {
  return audio_;
}

blink::WebSize WebMediaPlayerCastanets::NaturalSize() const {
  return blink::WebSize(natural_size_);
}

blink::WebSize WebMediaPlayerCastanets::VisibleRect() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  scoped_refptr<VideoFrame> video_frame = GetCurrentFrameFromCompositor();
  if (!video_frame)
    return blink::WebSize();

  const gfx::Rect& visible_rect = video_frame->visible_rect();
  return blink::WebSize(visible_rect.width(), visible_rect.height());
}

bool WebMediaPlayerCastanets::Paused() const {
  return is_paused_;
}

bool WebMediaPlayerCastanets::Seeking() const {
  return is_seeking_;
}

double WebMediaPlayerCastanets::Duration() const {
  return duration_.InSecondsF();
}

double WebMediaPlayerCastanets::CurrentTime() const {
  if (Seeking())
    return (pending_seek_ ? pending_seek_time_.InSecondsF()
                          : seek_time_.InSecondsF());
  return current_time_.InSecondsF();
}

void WebMediaPlayerCastanets::SuspendAndReleaseResources() {
  LOG(INFO) << __FUNCTION__ << " Player[" << player_id_ << "]";
  if (player_type_ == MEDIA_PLAYER_TYPE_NONE) {
    // TODO(m.debski): This should not happen as HTMLMediaElement is handling a
    // load deferral.
    LOG(ERROR) << "Player type is not set, load has not occured and there is "
                  "no player yet the player should suspend.";
    return;
  }

  if (!is_paused_)
    OnPauseStateChange(true);

  ReleaseMediaResource();
}

void WebMediaPlayerCastanets::Resume() {
  LOG(INFO) << __FUNCTION__ << "Player[" << player_id_ << "]";
  InitializeMediaResource();
}

blink::WebMediaPlayer::NetworkState WebMediaPlayerCastanets::GetNetworkState()
    const {
  return network_state_;
}

blink::WebMediaPlayer::ReadyState WebMediaPlayerCastanets::GetReadyState()
    const {
  return ready_state_;
}

blink::WebString WebMediaPlayerCastanets::GetErrorMessage() const {
  return blink::WebString::FromUTF8(media_log_->GetErrorMessage());
}

bool WebMediaPlayerCastanets::DidLoadingProgress() {
  if (did_loading_progress_) {
    did_loading_progress_ = false;
    return true;
  }
  return false;
}

bool WebMediaPlayerCastanets::DidGetOpaqueResponseFromServiceWorker() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebMediaPlayerCastanets::HasSingleSecurityOrigin() const {
  NOTIMPLEMENTED();
  return true;
}

bool WebMediaPlayerCastanets::DidPassCORSAccessCheck() const {
  NOTIMPLEMENTED();
  return false;
}

double WebMediaPlayerCastanets::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerCastanets::DecodedFrameCount() const {
  NOTIMPLEMENTED();
  return 0;
}

unsigned WebMediaPlayerCastanets::DroppedFrameCount() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerCastanets::AudioDecodedByteCount() const {
  NOTIMPLEMENTED();
  return 0;
}

size_t WebMediaPlayerCastanets::VideoDecodedByteCount() const {
  NOTIMPLEMENTED();
  return 0;
};

bool WebMediaPlayerCastanets::CopyVideoTextureToPlatformTexture(
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
    VideoFrameUploadMetadata* out_metadata) {
  NOTIMPLEMENTED();
  return false;
}

void WebMediaPlayerCastanets::SetReadyState(
    blink::WebMediaPlayer::ReadyState state) {
  ready_state_ = state;
  client_->ReadyStateChanged();
}

void WebMediaPlayerCastanets::SetNetworkState(
    blink::WebMediaPlayer::NetworkState state) {
  network_state_ = state;
  client_->NetworkStateChanged();
}

void WebMediaPlayerCastanets::SetContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm,
    blink::WebContentDecryptionModuleResult result) {}

void WebMediaPlayerCastanets::EnteredFullscreen() {
  if (is_fullscreen_)
    return;

  is_fullscreen_ = true;

  manager_->EnteredFullscreen(player_id_);
  if (HasVideo()) {
    CreateVideoHoleFrame();
  }
}

void WebMediaPlayerCastanets::ExitedFullscreen() {
  if (!is_fullscreen_)
    return;

  is_fullscreen_ = false;

  if (HasVideo()) {
    gfx::Size size(video_width_, video_height_);
    scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateBlackFrame(size);
    FrameReady(video_frame);
  }

  manager_->ExitedFullscreen(player_id_);
  client_->Repaint();
}

void WebMediaPlayerCastanets::FrameReady(
    const scoped_refptr<VideoFrame>& frame) {
  compositor_->PaintSingleFrame(frame);
}

void WebMediaPlayerCastanets::CreateVideoHoleFrame() {
  gfx::Size size(video_width_, video_height_);

  scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateHoleFrame(size);
  if (video_frame)
    FrameReady(video_frame);
}

void WebMediaPlayerCastanets::OnDrawableContentRectChanged(gfx::Rect rect,
                                                           bool is_video) {
  LOG(INFO) << __FUNCTION__ << "Player[" << player_id_
            << "] rect :" << rect.ToString();
  is_draw_ready_ = true;

  StopLayerBoundUpdateTimer();
  gfx::RectF rect_f = static_cast<gfx::RectF>(rect);
  if (manager_)
    manager_->SetMediaGeometry(player_id_, rect_f);

  if (pending_play_)
    Play();
}

bool WebMediaPlayerCastanets::UpdateBoundaryRectangle() {
  if (!video_layer_)
    return false;

  // Compute the geometry of video frame layer.
  cc::Layer* layer = video_layer_.get();
  gfx::RectF rect(gfx::SizeF(layer->bounds()));
  while (layer) {
    rect.Offset(layer->position().OffsetFromOrigin());
    rect.Offset(layer->CurrentScrollOffset().x() * (-1),
                layer->CurrentScrollOffset().y() * (-1));
    layer = layer->parent();
  }

  // Compute the real pixs if frame scaled.
  rect.Scale(frame_->View()->PageScaleFactor());

  // Return false when the geometry hasn't been changed from the last time.
  if (last_computed_rect_ == rect)
    return false;

  // Store the changed geometry information when it is actually changed.
  last_computed_rect_ = rect;
  return true;
}

const gfx::RectF WebMediaPlayerCastanets::GetBoundaryRectangle() {
  LOG(INFO) << __FUNCTION__ << "Player[" << player_id_
            << "] rect :" << last_computed_rect_.ToString();
  return last_computed_rect_;
}

void WebMediaPlayerCastanets::StartLayerBoundUpdateTimer() {
  if (layer_bound_update_timer_.IsRunning())
    return;

  layer_bound_update_timer_.Start(
      FROM_HERE, kLayerBoundUpdateInterval, this,
      &WebMediaPlayerCastanets::OnLayerBoundUpdateTimerFired);
}

void WebMediaPlayerCastanets::StopLayerBoundUpdateTimer() {
  if (layer_bound_update_timer_.IsRunning())
    layer_bound_update_timer_.Stop();
}

void WebMediaPlayerCastanets::OnLayerBoundUpdateTimerFired() {
  if (UpdateBoundaryRectangle()) {
    if (manager_) {
      manager_->SetMediaGeometry(player_id_, GetBoundaryRectangle());
      StopLayerBoundUpdateTimer();
    }
  }
}

void WebMediaPlayerCastanets::OnMediaDataChange(int width,
                                                int height,
                                                int media) {
  video_height_ = height;
  video_width_ = width;
  audio_ = media & static_cast<int>(MediaType::Audio) ? true : false;
  video_ = media & static_cast<int>(MediaType::Video) ? true : false;
  natural_size_ = gfx::Size(width, height);
  if (HasVideo() && !video_layer_) {
    video_layer_ =
        cc::VideoLayer::Create(compositor_.get(), media::VIDEO_ROTATION_0);
    video_layer_->SetContentsOpaque(opaque_);
    client_->SetCcLayer(video_layer_.get());
  }

  CreateVideoHoleFrame();
  StartLayerBoundUpdateTimer();
}

void WebMediaPlayerCastanets::OnTimeChanged() {
  client_->TimeChanged();
}

void WebMediaPlayerCastanets::OnDurationChange(base::TimeDelta Duration) {
  duration_ = Duration;
  client_->DurationChanged();
}

void WebMediaPlayerCastanets::OnNaturalSizeChanged(gfx::Size size) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, blink::WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent(
      media_log_->CreateVideoSizeSetEvent(size.width(), size.height()));
  natural_size_ = size;

  client_->SizeChanged();
}

void WebMediaPlayerCastanets::OnOpacityChanged(bool opaque) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, blink::WebMediaPlayer::kReadyStateHaveNothing);

  opaque_ = opaque;
  if (video_layer_)
    video_layer_->SetContentsOpaque(opaque_);
}

scoped_refptr<VideoFrame>
WebMediaPlayerCastanets::GetCurrentFrameFromCompositor() const {
  // Can be null.
  scoped_refptr<VideoFrame> video_frame =
      compositor_->GetCurrentFrameOnAnyThread();

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&media::VideoFrameCompositor::UpdateCurrentFrameIfStale,
                 base::Unretained(compositor_.get())));

  return video_frame;
}

void WebMediaPlayerCastanets::OnTimeUpdate(base::TimeDelta current_time) {
  current_time_ = current_time;
}

void WebMediaPlayerCastanets::OnBufferingUpdate(int percentage) {
  buffered_[0].end = Duration() * percentage / 100;
  did_loading_progress_ = true;
}

void WebMediaPlayerCastanets::OnPauseStateChange(bool state) {
  if (is_paused_ == state)
    return;

  is_paused_ = state;
  if (is_paused_)
    client_->RequestPause();
  else
    client_->RequestPlay();

  if (!delegate_)
    return;

  if (is_paused_) {
    delegate_->DidPause(delegate_id_);
  } else {
    delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                       media::DurationToMediaContentType(duration_));
  }
}

void WebMediaPlayerCastanets::OnPlayerSuspend(bool is_preempted) {
  if (!is_paused_ && is_preempted) {
    OnPauseStateChange(true);
  }

  if (!delegate_)
    return;
  delegate_->PlayerGone(delegate_id_);
}

void WebMediaPlayerCastanets::OnPlayerResumed(bool is_preempted) {
  if (!delegate_)
    return;

  if (is_paused_)
    delegate_->DidPause(delegate_id_);
  else
    delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                       media::DurationToMediaContentType(duration_));
}

void WebMediaPlayerCastanets::OnPlayerDestroyed() {
  NOTIMPLEMENTED();
}

void WebMediaPlayerCastanets::OnSeekComplete() {
  LOG(INFO) << __FUNCTION__ << "Player[" << player_id_ << "]"
            << " seconds :" << seek_time_.InSecondsF();
  is_seeking_ = false;
  seek_time_ = base::TimeDelta();

  CreateVideoHoleFrame();
  client_->TimeChanged();
}

void WebMediaPlayerCastanets::OnSeekRequest(base::TimeDelta seek_time) {
  client_->RequestSeek(seek_time.InSecondsF());
}

}  // namespace content
