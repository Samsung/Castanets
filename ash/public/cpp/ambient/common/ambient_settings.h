// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_
#define ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string16.h"

namespace ash {

// Structs and classes related to Ambient mode Settings.

// Enumeration of the topic source, i.e. where the photos come from.
// Values need to stay in sync with the |topicSource_| in ambient_mode_page.js.
// Art gallery is a super set of art related topic sources in Backdrop service.
enum class AmbientModeTopicSource {
  kMinValue = 0,
  kGooglePhotos = kMinValue,
  kArtGallery = 1,
  kMaxValue = kArtGallery
};

// Subsettings of Art gallery.
struct ASH_PUBLIC_EXPORT ArtSetting {
  ArtSetting();
  ArtSetting(const ArtSetting&);
  ArtSetting(ArtSetting&&);
  ArtSetting& operator=(const ArtSetting&);
  ArtSetting& operator=(ArtSetting&&);
  ~ArtSetting();

  int setting_id = 0;

  // Whether the setting is enabled in the Art gallery topic source.
  bool enabled = false;

  // UTF-8 encoded.
  std::string title;

  // UTF-8 encoded.
  std::string description;

  std::string preview_image_url;
};

struct ASH_PUBLIC_EXPORT AmbientSettings {
  AmbientSettings();
  AmbientSettings(const AmbientSettings&);
  AmbientSettings(AmbientSettings&&);
  AmbientSettings& operator=(const AmbientSettings&);
  AmbientSettings& operator=(AmbientSettings&&);
  ~AmbientSettings();

  AmbientModeTopicSource topic_source;

  // Only a subset Settings of Art gallery.
  std::vector<ArtSetting> art_settings;

  // Only selected album.
  std::vector<std::string> selected_album_ids;
};

struct ASH_PUBLIC_EXPORT PersonalAlbum {
  PersonalAlbum();
  PersonalAlbum(const PersonalAlbum&) = delete;
  PersonalAlbum(PersonalAlbum&&);
  PersonalAlbum& operator=(const PersonalAlbum&) = delete;
  PersonalAlbum& operator=(PersonalAlbum&&);
  ~PersonalAlbum();

  // ID of this album.
  std::string album_id;

  // Whether the album is selected in the Google Photos topic source.
  bool selected = false;

  // UTF-8 encoded.
  std::string album_name;

  // UTF-8 encoded.
  std::string description;

  // Preview image of this album.
  std::string banner_image_url;

  // Preview images if this is a live album.
  std::vector<std::string> preview_image_urls;
};

struct ASH_PUBLIC_EXPORT PersonalAlbums {
  PersonalAlbums();
  PersonalAlbums(const PersonalAlbums&) = delete;
  PersonalAlbums(PersonalAlbums&&);
  PersonalAlbums& operator=(const PersonalAlbums&) = delete;
  PersonalAlbums& operator=(PersonalAlbums&&);
  ~PersonalAlbums();

  std::vector<PersonalAlbum> albums;

  // A token that the client application can use to retrieve the next batch of
  // albums. If the token is not set in the response, it means that there are
  // no more albums.
  std::string resume_token;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_COMMON_AMBIENT_SETTINGS_H_
