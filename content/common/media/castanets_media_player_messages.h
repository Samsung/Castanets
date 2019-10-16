// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for castanets player.
#ifndef CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PLAYER_MESSAGES_H_
#define CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PLAYER_MESSAGES_H_

#include "content/common/media/castanets_media_param_traits.h"
#include "content/common/media/castanets_media_player_init_config.h"
#include "ipc/ipc_message_macros.h"
#include "media/blink/renderer_media_player_interface.h"
#include "ui/gfx/geometry/rect_f.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerCastanetsMsgStart

IPC_ENUM_TRAITS(blink::WebMediaPlayer::ReadyState)
IPC_ENUM_TRAITS(blink::WebMediaPlayer::NetworkState)
#if !defined(OS_ANDROID)
IPC_ENUM_TRAITS(MediaPlayerHostMsg_Initialize_Type)
#endif

// Should be same as castanets player.
IPC_STRUCT_TRAITS_BEGIN(content::MediaPlayerInitConfig)
IPC_STRUCT_TRAITS_MEMBER(type)
IPC_STRUCT_TRAITS_MEMBER(url)
IPC_STRUCT_TRAITS_MEMBER(mime_type)
IPC_STRUCT_TRAITS_MEMBER(demuxer_client_id)
IPC_STRUCT_TRAITS_MEMBER(has_encrypted_listener_or_cdm)
IPC_STRUCT_TRAITS_END()

// TODO (sm.venugopal): Rename the IPCs.
// Initialize Efl player.
IPC_MESSAGE_ROUTED2(MediaPlayerEflHostMsg_Init,
                    int /* player_id */,
                    content::MediaPlayerInitConfig /* config */)

// Requests the player to enter fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_EnteredFullscreen,
                    int /* player_id */)

// Requests the player to exit fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_ExitedFullscreen, int /* player_id */)

// Deinitialize Gst player.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_DeInit, int /* player_id */)

// Start playback.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_Play, int /* player_id */)

// Pause playback.
IPC_MESSAGE_ROUTED2(MediaPlayerEflHostMsg_Pause,
                    int /* player_id */,
                    bool /* is_media_related_action */)

// Suspend media player.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_Suspend, int /* player_id */)

// Resume media player.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_Resume, int /* player_id*/)

// Player was activated by an user or an app.
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_Activate, int /* player_id*/)

// Player should deactivate (ex. save power).
IPC_MESSAGE_ROUTED1(MediaPlayerEflHostMsg_Deactivate, int /* player_id*/)

// Set volume.
IPC_MESSAGE_ROUTED2(MediaPlayerEflHostMsg_SetVolume,
                    int /* player_id */,
                    double /* volume */)

// Set playback rate.
IPC_MESSAGE_ROUTED2(MediaPlayerEflHostMsg_SetRate,
                    int /* player_id */,
                    double /* rate */)

// Playback duration.
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_DurationChanged,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Current  duration.
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_TimeUpdate,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Pause state.
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_PauseStateChanged,
                    int /* player_id */,
                    bool /* state */)

// Seek state.
IPC_MESSAGE_ROUTED1(MediaPlayerEflMsg_OnSeekComplete, int /* player_id */)

// Current buffer range.
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_BufferUpdate,
                    int /* player_id */,
                    int /* buffering_percentage */)

// Playback completed.
IPC_MESSAGE_ROUTED1(MediaPlayerEflMsg_TimeChanged, int /* player_id */)

IPC_MESSAGE_ROUTED1(MediaPlayerEflMsg_PlayerDestroyed, int /* player_id */)

// Ready state change.
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_ReadyStateChange,
                    int /* player_id */,
                    blink::WebMediaPlayer::ReadyState /* state */)

// Network state change.
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_NetworkStateChange,
                    int /* player_id */,
                    blink::WebMediaPlayer::NetworkState /* state */)

// Gst media data has changed.
IPC_MESSAGE_ROUTED4(MediaPlayerEflMsg_MediaDataChanged,
                    int /* player_id */,
                    int /* width */,
                    int /* height */,
                    int /* media */)

// Set geometry.
IPC_MESSAGE_ROUTED2(MediaPlayerEflHostMsg_SetGeometry,
                    int /* player_id */,
                    gfx::RectF /* position and size */)
// Seek.
IPC_MESSAGE_ROUTED2(MediaPlayerEflHostMsg_Seek,
                    int /* player_id */,
                    base::TimeDelta /* time */)

IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_SeekRequest,
                    int /* player_id */,
                    base::TimeDelta /* time_to_seek */)

// Player has begun suspend procedure
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_PlayerSuspend,
                    int /* player_id */,
                    bool /* is_preempted */)

// Player has resumed
IPC_MESSAGE_ROUTED2(MediaPlayerEflMsg_PlayerResumed,
                    int /* player_id */,
                    bool /* is_preempted */)

#endif  // CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PLAYER_MESSAGES_H_
