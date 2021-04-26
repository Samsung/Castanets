// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_application_info.h"

// WebApplicationIconInfo
WebApplicationIconInfo::WebApplicationIconInfo() = default;

WebApplicationIconInfo::WebApplicationIconInfo(const WebApplicationIconInfo&) =
    default;

WebApplicationIconInfo::WebApplicationIconInfo(WebApplicationIconInfo&&) =
    default;

WebApplicationIconInfo::~WebApplicationIconInfo() = default;

WebApplicationIconInfo& WebApplicationIconInfo::operator=(
    const WebApplicationIconInfo&) = default;

WebApplicationIconInfo& WebApplicationIconInfo::operator=(
    WebApplicationIconInfo&&) = default;

// WebApplicationShortcutsMenuItemInfo::Icon
WebApplicationShortcutsMenuItemInfo::Icon::Icon() = default;

WebApplicationShortcutsMenuItemInfo::Icon::Icon(
    const WebApplicationShortcutsMenuItemInfo::Icon&) = default;

WebApplicationShortcutsMenuItemInfo::Icon::Icon(
    WebApplicationShortcutsMenuItemInfo::Icon&&) = default;

WebApplicationShortcutsMenuItemInfo::Icon::~Icon() = default;

WebApplicationShortcutsMenuItemInfo::Icon&
WebApplicationShortcutsMenuItemInfo::Icon::operator=(
    const WebApplicationShortcutsMenuItemInfo::Icon&) = default;

WebApplicationShortcutsMenuItemInfo::Icon&
WebApplicationShortcutsMenuItemInfo::Icon::operator=(
    WebApplicationShortcutsMenuItemInfo::Icon&&) = default;

// WebApplicationShortcutsMenuItemInfo
WebApplicationShortcutsMenuItemInfo::WebApplicationShortcutsMenuItemInfo() =
    default;

WebApplicationShortcutsMenuItemInfo::WebApplicationShortcutsMenuItemInfo(
    const WebApplicationShortcutsMenuItemInfo& other) = default;

WebApplicationShortcutsMenuItemInfo::WebApplicationShortcutsMenuItemInfo(
    WebApplicationShortcutsMenuItemInfo&&) noexcept = default;

WebApplicationShortcutsMenuItemInfo::~WebApplicationShortcutsMenuItemInfo() =
    default;

WebApplicationShortcutsMenuItemInfo&
WebApplicationShortcutsMenuItemInfo::operator=(
    const WebApplicationShortcutsMenuItemInfo&) = default;

WebApplicationShortcutsMenuItemInfo&
WebApplicationShortcutsMenuItemInfo::operator=(
    WebApplicationShortcutsMenuItemInfo&&) noexcept = default;

// WebApplicationInfo
WebApplicationInfo::WebApplicationInfo() = default;

WebApplicationInfo::WebApplicationInfo(const WebApplicationInfo& other) =
    default;

WebApplicationInfo::~WebApplicationInfo() = default;

bool operator==(const WebApplicationIconInfo& icon_info1,
                const WebApplicationIconInfo& icon_info2) {
  return std::tie(icon_info1.url, icon_info1.square_size_px) ==
         std::tie(icon_info2.url, icon_info2.square_size_px);
}

std::ostream& operator<<(std::ostream& out,
                         const WebApplicationIconInfo& icon_info) {
  out << "url: " << icon_info.url << " square_size_px: ";
  if (icon_info.square_size_px)
    out << *icon_info.square_size_px;
  else
    out << "none";
  return out;
}

bool operator==(const WebApplicationShortcutsMenuItemInfo::Icon& icon1,
                const WebApplicationShortcutsMenuItemInfo::Icon& icon2) {
  return std::tie(icon1.url, icon1.square_size_px) ==
         std::tie(icon2.url, icon2.square_size_px);
}

bool operator==(const WebApplicationShortcutsMenuItemInfo& shortcut_info1,
                const WebApplicationShortcutsMenuItemInfo& shortcut_info2) {
  return std::tie(shortcut_info1.name, shortcut_info1.url,
                  shortcut_info1.shortcut_icon_infos) ==
         std::tie(shortcut_info2.name, shortcut_info2.url,
                  shortcut_info2.shortcut_icon_infos);
}
