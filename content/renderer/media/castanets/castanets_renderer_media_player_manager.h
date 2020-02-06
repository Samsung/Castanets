// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CASTANETS_CASTANETS_RENDERER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_CASTANETS_CASTANETS_RENDERER_MEDIA_PLAYER_MANAGER_H_

#include <map>
#include "content/public/renderer/render_frame_observer.h"
#include "media/blink/renderer_media_player_interface.h"

namespace content {

class CastanetsRendererMediaPlayerManager
    : public RenderFrameObserver,
      public media::RendererMediaPlayerManagerInterface {
 public:
  // Constructs a CastanetsRendererMediaPlayerManager object for the
  // |render_frame|.
  explicit CastanetsRendererMediaPlayerManager(RenderFrame* render_frame);
  ~CastanetsRendererMediaPlayerManager() override;

  // RendererMediaPlayerManagerInterface overrides.
  void Initialize(int player_id,
                  MediaPlayerHostMsg_Initialize_Type type,
                  const GURL& url,
                  const std::string& mime_type,
                  int demuxer_client_id) override;
  void Start(int player_id) override;

  // Pauses the player.
  // is_media_related_action should be true if this pause is coming from an
  // an action that explicitly pauses the video (user pressing pause, JS, etc.)
  // Otherwise it should be false if Pause is being called due to other reasons
  // (cleanup, freeing resources, etc.)
  void Pause(int player_id, bool is_media_related_action) override;
  void Seek(int player_id, base::TimeDelta time) override;
  void SetVolume(int player_id, double volume) override;
  void SetRate(int player_id, double rate) override;

  // Registers and unregisters a WebMediaPlayerEfl object.
  int RegisterMediaPlayer(media::RendererMediaPlayerInterface* player) override;
  void UnregisterMediaPlayer(int player_id) override;

  void DestroyPlayer(int player_id) override;
  void Suspend(int player_id) override {}
  void Resume(int player_id) override {}
  void Activate(int player_id) override {}
  void Deactivate(int player_id) override {}

  // Requests the player to enter/exit fullscreen.
  void EnteredFullscreen(int player_id) override;
  void ExitedFullscreen(int player_id) override;

  void SetMediaGeometry(int player_id, const gfx::RectF& rect) override;

  void Initialize(MediaPlayerHostMsg_Initialize_Type type,
                  int player_id,
                  const GURL& url,
                  const GURL& site_for_cookies,
                  const GURL& frame_url,
                  bool allow_credentials,
                  int delegate_id) override {}

  void SetPoster(int player_id, const GURL& poster) override {}
  void SuspendAndReleaseResources(int player_id) override {}
  void RequestRemotePlayback(int player_id) override {}
  void RequestRemotePlaybackControl(int player_id) override {}
  void RequestRemotePlaybackStop(int player_id) override {}

  // RenderFrameObserver overrides.
  void OnDestruct() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void WasHidden() override {}
  void WasShown() override {}
  void OnStop() override {}

 private:
  void OnPlayerDestroyed(int player_id);
  void OnMediaDataChange(int player_id, int width, int height, int media);
  void OnDurationChange(int player_id, base::TimeDelta duration);
  void OnTimeUpdate(int player_id, base::TimeDelta current_time);
  void OnBufferUpdate(int player_id, int percentage);
  void OnTimeChanged(int player_id);
  void OnPauseStateChange(int player_id, bool state);
  void OnSeekComplete(int player_id);
  void OnRequestSeek(int player_id, base::TimeDelta seek_time);
  void OnPlayerSuspend(int player_id, bool is_preempted);
  void OnPlayerResumed(int player_id, bool is_preempted);
  void OnReadyStateChange(int player_id,
                          blink::WebMediaPlayer::ReadyState state);
  void OnNetworkStateChange(int player_id,
                            blink::WebMediaPlayer::NetworkState state);

  media::RendererMediaPlayerInterface* GetMediaPlayer(int player_id);

  // Pause the playing media players when tab/webpage goes to background
  void PausePlayingPlayers();

  std::map<int, media::RendererMediaPlayerInterface*> media_players_;

  DISALLOW_COPY_AND_ASSIGN(CastanetsRendererMediaPlayerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CASTANETS_CASTANETS_RENDERER_MEDIA_PLAYER_MANAGER_H_
