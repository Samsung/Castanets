// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/castanets/castanets_renderer_media_player_manager.h"

#include "content/common/media/castanets_media_player_messages.h"

namespace content {

CastanetsRendererMediaPlayerManager::CastanetsRendererMediaPlayerManager(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

CastanetsRendererMediaPlayerManager::~CastanetsRendererMediaPlayerManager() {
  DCHECK(media_players_.empty())
      << "CastanetsRendererMediaPlayerManager is owned by RenderFrameImpl and "
         "is "
         "destroyed only after all media players are destroyed.";
}

int CastanetsRendererMediaPlayerManager::RegisterMediaPlayer(
    media::RendererMediaPlayerInterface* player) {
  // Note : For the unique player id among the all renderer process,
  // generate player id based on renderer process id.
  static int next_media_player_id_ = base::GetCurrentProcId() << 16;
  next_media_player_id_ = (next_media_player_id_ & 0xFFFF0000) |
                          ((next_media_player_id_ + 1) & 0x0000FFFF);
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_;
}

void CastanetsRendererMediaPlayerManager::UnregisterMediaPlayer(int player_id) {
  media_players_.erase(player_id);
}

media::RendererMediaPlayerInterface*
CastanetsRendererMediaPlayerManager::GetMediaPlayer(int player_id) {
  std::map<int, media::RendererMediaPlayerInterface*>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return iter->second;
  return NULL;
}

void CastanetsRendererMediaPlayerManager::OnDestruct() {
  delete this;
}

bool CastanetsRendererMediaPlayerManager::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CastanetsRendererMediaPlayerManager, message)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_MediaDataChanged, OnMediaDataChange)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_DurationChanged, OnDurationChange)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_TimeUpdate, OnTimeUpdate)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_BufferUpdate, OnBufferUpdate)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_ReadyStateChange, OnReadyStateChange)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_NetworkStateChange,
                      OnNetworkStateChange)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_TimeChanged, OnTimeChanged)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_PauseStateChanged, OnPauseStateChange)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_OnSeekComplete, OnSeekComplete)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_SeekRequest, OnRequestSeek)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_PlayerSuspend, OnPlayerSuspend)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_PlayerResumed, OnPlayerResumed)
  IPC_MESSAGE_HANDLER(MediaPlayerEflMsg_PlayerDestroyed, OnPlayerDestroyed)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CastanetsRendererMediaPlayerManager::Initialize(
    int player_id,
    MediaPlayerHostMsg_Initialize_Type type,
    const GURL& url,
    const std::string& mime_type,
    int demuxer_client_id) {
  bool has_encrypted_listener_or_cdm = false;
  MediaPlayerInitConfig config{type, url, mime_type, demuxer_client_id,
                               has_encrypted_listener_or_cdm};
  Send(new MediaPlayerEflHostMsg_Init(routing_id(), player_id, config));
}

void CastanetsRendererMediaPlayerManager::OnMediaDataChange(int player_id,
                                                            int width,
                                                            int height,
                                                            int media) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaDataChange(width, height, media);
}

void CastanetsRendererMediaPlayerManager::OnPlayerDestroyed(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPlayerDestroyed();
}

void CastanetsRendererMediaPlayerManager::OnDurationChange(
    int player_id,
    base::TimeDelta duration) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDurationChange(duration);
}

void CastanetsRendererMediaPlayerManager::OnTimeUpdate(
    int player_id,
    base::TimeDelta current_time) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_time);
}

void CastanetsRendererMediaPlayerManager::OnBufferUpdate(int player_id,
                                                         int percentage) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnBufferingUpdate(percentage);
}

void CastanetsRendererMediaPlayerManager::OnReadyStateChange(
    int player_id,
    blink::WebMediaPlayer::ReadyState state) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->SetReadyState(
        static_cast<blink::WebMediaPlayer::ReadyState>(state));
}

void CastanetsRendererMediaPlayerManager::OnNetworkStateChange(
    int player_id,
    blink::WebMediaPlayer::NetworkState state) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->SetNetworkState(
        static_cast<blink::WebMediaPlayer::NetworkState>(state));
}

void CastanetsRendererMediaPlayerManager::OnTimeChanged(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnTimeChanged();
}

void CastanetsRendererMediaPlayerManager::OnSeekComplete(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete();
}

void CastanetsRendererMediaPlayerManager::OnPauseStateChange(int player_id,
                                                             bool state) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPauseStateChange(state);
}

void CastanetsRendererMediaPlayerManager::OnRequestSeek(
    int player_id,
    base::TimeDelta seek_time) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekRequest(seek_time);
}

void CastanetsRendererMediaPlayerManager::OnPlayerSuspend(int player_id,
                                                          bool is_preempted) {
  if (media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id))
    player->OnPlayerSuspend(is_preempted);
}

void CastanetsRendererMediaPlayerManager::OnPlayerResumed(int player_id,
                                                          bool is_preempted) {
  if (media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id))
    player->OnPlayerResumed(is_preempted);
}

void CastanetsRendererMediaPlayerManager::Start(int player_id) {
  Send(new MediaPlayerEflHostMsg_Play(routing_id(), player_id));
}

void CastanetsRendererMediaPlayerManager::Pause(int player_id,
                                                bool is_media_related_action) {
  Send(new MediaPlayerEflHostMsg_Pause(routing_id(), player_id,
                                       is_media_related_action));
}

void CastanetsRendererMediaPlayerManager::Seek(int player_id,
                                               base::TimeDelta time) {
  Send(new MediaPlayerEflHostMsg_Seek(routing_id(), player_id, time));
}

void CastanetsRendererMediaPlayerManager::SetVolume(int player_id,
                                                    double volume) {
  Send(new MediaPlayerEflHostMsg_SetVolume(routing_id(), player_id, volume));
}

void CastanetsRendererMediaPlayerManager::SetRate(int player_id, double rate) {
  Send(new MediaPlayerEflHostMsg_SetRate(routing_id(), player_id, rate));
}

void CastanetsRendererMediaPlayerManager::DestroyPlayer(int player_id) {
  Send(new MediaPlayerEflHostMsg_DeInit(routing_id(), player_id));
}

void CastanetsRendererMediaPlayerManager::EnteredFullscreen(int player_id) {
  Send(new MediaPlayerEflHostMsg_EnteredFullscreen(routing_id(), player_id));
}

void CastanetsRendererMediaPlayerManager::ExitedFullscreen(int player_id) {
  Send(new MediaPlayerEflHostMsg_ExitedFullscreen(routing_id(), player_id));
}

void CastanetsRendererMediaPlayerManager::SetMediaGeometry(
    int player_id,
    const gfx::RectF& rect) {
  gfx::RectF video_rect = rect;
  Send(new MediaPlayerEflHostMsg_SetGeometry(routing_id(), player_id,
                                             video_rect));
}

}  // namespace content
