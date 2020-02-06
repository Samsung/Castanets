// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PLAYER_INIT_CONFIG_H_
#define CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PLAYER_INIT_CONFIG_H_

#include <string>
#include "media/blink/renderer_media_player_interface.h"
#include "url/gurl.h"

namespace content {

struct MediaPlayerInitConfig {
  MediaPlayerHostMsg_Initialize_Type type;
  GURL url;
  std::string mime_type;
  int demuxer_client_id;
  bool has_encrypted_listener_or_cdm;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PLAYER_INIT_CONFIG_H_
