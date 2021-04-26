// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_icon_manager.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_app_shortcut_icons_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

void OnExtensionIconLoaded(BookmarkAppIconManager::ReadIconCallback callback,
                           const gfx::Image& image) {
  std::move(callback).Run(image.IsEmpty() ? SkBitmap() : *image.ToSkBitmap());
}

const Extension* GetBookmarkApp(Profile* profile,
                                const web_app::AppId& app_id) {
  const Extension* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(app_id);
  return (extension && extension->from_bookmark()) ? extension : nullptr;
}

void ReadExtensionIcon(Profile* profile,
                       const web_app::AppId& app_id,
                       SquareSizePx icon_size_in_px,
                       ExtensionIconSet::MatchType match_type,
                       BookmarkAppIconManager::ReadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* extension = GetBookmarkApp(profile, app_id);
  DCHECK(extension);

  ImageLoader* loader = ImageLoader::Get(profile);
  loader->LoadImageAsync(
      extension,
      IconsInfo::GetIconResource(extension, icon_size_in_px, match_type),
      gfx::Size(icon_size_in_px, icon_size_in_px),
      base::BindOnce(&OnExtensionIconLoaded, std::move(callback)));
}

void OnExtensionIconsLoaded(BookmarkAppIconManager::ReadIconsCallback callback,
                            const gfx::Image& image) {
  std::map<SquareSizePx, SkBitmap> icons_map;

  gfx::ImageSkia image_skia = image.AsImageSkia();
  for (const gfx::ImageSkiaRep& image_skia_rep : image_skia.image_reps())
    icons_map[image_skia_rep.pixel_width()] = image_skia_rep.GetBitmap();

  std::move(callback).Run(std::move(icons_map));
}

void ReadExtensionIcons(Profile* profile,
                        const web_app::AppId& app_id,
                        const std::vector<SquareSizePx>& icon_sizes_in_px,
                        BookmarkAppIconManager::ReadIconsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* app = GetBookmarkApp(profile, app_id);
  DCHECK(app);

  std::vector<ImageLoader::ImageRepresentation> info_list;
  for (SquareSizePx size_in_px : icon_sizes_in_px) {
    ExtensionResource resource = IconsInfo::GetIconResource(
        app, size_in_px, ExtensionIconSet::MATCH_EXACTLY);
    ImageLoader::ImageRepresentation image_rep{
        resource, ImageLoader::ImageRepresentation::NEVER_RESIZE,
        gfx::Size{size_in_px, size_in_px}, /*scale_factor=*/0.0f};
    info_list.push_back(image_rep);
  }

  ImageLoader* loader = ImageLoader::Get(profile);
  loader->LoadImagesAsync(
      app, info_list,
      base::BindOnce(&OnExtensionIconsLoaded, std::move(callback)));
}

SkBitmap ReadShortcutsMenuIconBlocking(const base::FilePath& path) {
  // Read icon data from disk.
  std::string icon_data;
  if (path.empty() || !base::ReadFileToString(path, &icon_data)) {
    return SkBitmap();
  }

  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(icon_data.c_str()),
          icon_data.size(), &bitmap)) {
    return SkBitmap();
  }

  return bitmap;
}

// Performs blocking I/O. May be called on another thread.
ShortcutsMenuIconsBitmaps ReadShortcutsMenuIconsBlocking(
    const std::vector<std::vector<ImageLoader::ImageRepresentation>>&
        shortcuts_menu_images_reps) {
  ShortcutsMenuIconsBitmaps results;
  for (const auto& image_reps : shortcuts_menu_images_reps) {
    std::map<SquareSizePx, SkBitmap> result;
    for (const auto& image_rep : image_reps) {
      SkBitmap bitmap =
          ReadShortcutsMenuIconBlocking(image_rep.resource.GetFilePath());
      if (!bitmap.empty())
        result[image_rep.desired_size.width()] = std::move(bitmap);
    }
    // We always push_back (even when result is empty) to keep a given
    // std::map's index in sync with that of its corresponding shortcuts menu
    // item.
    results.push_back(std::move(result));
  }
  return results;
}

std::vector<std::vector<ImageLoader::ImageRepresentation>>
CreateShortcutsMenuIconsImageRepresentations(
    Profile* profile,
    const web_app::AppId& app_id,
    const std::vector<std::vector<SquareSizePx>>& shortcuts_menu_icons_sizes) {
  const Extension* web_app = GetBookmarkApp(profile, app_id);
  DCHECK(web_app);

  std::vector<std::vector<ImageLoader::ImageRepresentation>> results;
  for (size_t i = 0; i < shortcuts_menu_icons_sizes.size(); ++i) {
    std::vector<ImageLoader::ImageRepresentation> result;
    for (const auto& icon_size : shortcuts_menu_icons_sizes[i]) {
      ExtensionResource resource = WebAppShortcutIconsInfo::GetIconResource(
          web_app, i, icon_size, ExtensionIconSet::MATCH_EXACTLY);
      ImageLoader::ImageRepresentation image_rep{
          resource, ImageLoader::ImageRepresentation::NEVER_RESIZE,
          gfx::Size{icon_size, icon_size}, /*scale_factor=*/0.0f};
      result.emplace_back(std::move(image_rep));
    }
    results.emplace_back(std::move(result));
  }
  return results;
}

}  // anonymous namespace

BookmarkAppIconManager::BookmarkAppIconManager(Profile* profile)
    : profile_(profile) {}

BookmarkAppIconManager::~BookmarkAppIconManager() = default;

void BookmarkAppIconManager::Start() {}

void BookmarkAppIconManager::Shutdown() {}

bool BookmarkAppIconManager::HasIcons(
    const web_app::AppId& app_id,
    const std::vector<SquareSizePx>& icon_sizes_in_px) const {
  const Extension* app = GetBookmarkApp(profile_, app_id);
  if (!app)
    return false;

  const ExtensionIconSet& icons = IconsInfo::GetIcons(app);

  for (SquareSizePx size_in_px : icon_sizes_in_px) {
    const std::string& path =
        icons.Get(size_in_px, ExtensionIconSet::MATCH_EXACTLY);
    if (path.empty())
      return false;
  }

  return true;
}

bool BookmarkAppIconManager::HasSmallestIcon(
    const web_app::AppId& app_id,
    SquareSizePx icon_size_in_px) const {
  const Extension* app = GetBookmarkApp(profile_, app_id);
  if (!app)
    return false;

  const ExtensionIconSet& icons = IconsInfo::GetIcons(app);

  const std::string& path =
      icons.Get(icon_size_in_px, ExtensionIconSet::MATCH_BIGGER);

  return !path.empty();
}

void BookmarkAppIconManager::ReadIcons(
    const web_app::AppId& app_id,
    const std::vector<SquareSizePx>& icon_sizes_in_px,
    ReadIconsCallback callback) const {
  DCHECK(HasIcons(app_id, icon_sizes_in_px));
  ReadExtensionIcons(profile_, app_id, icon_sizes_in_px, std::move(callback));
}

void BookmarkAppIconManager::ReadAllIcons(const web_app::AppId& app_id,
                                          ReadIconsCallback callback) const {
  const Extension* app = GetBookmarkApp(profile_, app_id);
  DCHECK(app);
  ReadExtensionIcons(profile_, app_id, GetBookmarkAppDownloadedIconSizes(app),
                     std::move(callback));
}

void BookmarkAppIconManager::ReadAllShortcutsMenuIcons(
    const web_app::AppId& app_id,
    ReadShortcutsMenuIconsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* web_app = GetBookmarkApp(profile_, app_id);
  DCHECK(web_app);

  if (!web_app) {
    std::move(callback).Run(ShortcutsMenuIconsBitmaps{});
    return;
  }
  std::vector<std::vector<ImageLoader::ImageRepresentation>> img_reps =
      CreateShortcutsMenuIconsImageRepresentations(
          profile_, app_id,
          GetBookmarkAppDownloadedShortcutsMenuIconsSizes(web_app));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(ReadShortcutsMenuIconsBlocking, std::move(img_reps)),
      std::move(callback));
}

void BookmarkAppIconManager::ReadSmallestIcon(const web_app::AppId& app_id,
                                              SquareSizePx icon_size_in_px,
                                              ReadIconCallback callback) const {
  DCHECK(HasSmallestIcon(app_id, icon_size_in_px));
  ReadExtensionIcon(profile_, app_id, icon_size_in_px,
                    ExtensionIconSet::MATCH_BIGGER, std::move(callback));
}

void BookmarkAppIconManager::ReadSmallestCompressedIcon(
    const web_app::AppId& app_id,
    SquareSizePx icon_size_in_px,
    ReadCompressedIconCallback callback) const {
  NOTIMPLEMENTED();
  DCHECK(HasSmallestIcon(app_id, icon_size_in_px));
  std::move(callback).Run(std::vector<uint8_t>());
}

SkBitmap BookmarkAppIconManager::GetFavicon(
    const web_app::AppId& app_id) const {
  auto* menu_manager = extensions::MenuManager::Get(profile_);
  return menu_manager->GetIconForExtension(app_id).AsBitmap();
}

}  // namespace extensions
