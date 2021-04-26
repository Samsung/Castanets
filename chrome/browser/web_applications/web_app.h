// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <bitset>
#include <iosfwd>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/sync/model/string_ordinal.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace web_app {

class WebApp {
 public:
  explicit WebApp(const AppId& app_id);
  ~WebApp();

  // Copyable and move-assignable to support Copy-on-Write with Commit.
  WebApp(const WebApp& web_app);
  WebApp& operator=(WebApp&& web_app);

  // Explicitly disallow other copy ctors and assign operators.
  WebApp(WebApp&&) = delete;
  WebApp& operator=(const WebApp&) = delete;

  const AppId& app_id() const { return app_id_; }

  // UTF8 encoded application name.
  const std::string& name() const { return name_; }
  // UTF8 encoded long application description (a full application name).
  const std::string& description() const { return description_; }

  const GURL& launch_url() const { return launch_url_; }
  const GURL& scope() const { return scope_; }

  const base::Optional<SkColor>& theme_color() const { return theme_color_; }

  DisplayMode display_mode() const { return display_mode_; }

  DisplayMode user_display_mode() const { return user_display_mode_; }

  syncer::StringOrdinal user_page_ordinal() const { return user_page_ordinal_; }
  syncer::StringOrdinal user_launch_ordinal() const {
    return user_launch_ordinal_;
  }

  const base::Optional<WebAppChromeOsData>& chromeos_data() const {
    return chromeos_data_;
  }

  // Locally installed apps have shortcuts installed on various UI surfaces.
  // If app isn't locally installed, it is excluded from UIs and only listed as
  // a part of user's app library.
  bool is_locally_installed() const { return is_locally_installed_; }
  // Sync-initiated installation produces a stub app awaiting for full
  // installation process. The |is_in_sync_install| app has only app_id,
  // launch_url and sync_fallback_data fields defined, no icons. If online
  // install succeeds, icons get downloaded and all the fields get their values.
  // If online install fails, we do the fallback installation to generate icons
  // using |sync_fallback_data| fields.
  bool is_in_sync_install() const { return is_in_sync_install_; }

  // Represents the last time this app is launched.
  const base::Time& last_launch_time() const { return last_launch_time_; }
  // Represents the time when this app is installed.
  const base::Time& install_time() const { return install_time_; }

  // Represents the "icons" field in the manifest.
  const std::vector<WebApplicationIconInfo>& icon_infos() const {
    return icon_infos_;
  }

  // Represents which icon sizes we successfully downloaded from the icon_infos.
  // Icon sizes are sorted in ascending order.
  const std::vector<SquareSizePx>& downloaded_icon_sizes() const {
    return downloaded_icon_sizes_;
  }

  const apps::FileHandlers& file_handlers() const { return file_handlers_; }

  const std::vector<std::string>& additional_search_terms() const {
    return additional_search_terms_;
  }

  // While local |name| and |theme_color| may vary from device to device, the
  // synced copies of these fields are replicated to all devices. The synced
  // copies are read by a device to generate a placeholder icon (if needed). Any
  // device may write new values to |sync_fallback_data|, random last update
  // wins.
  struct SyncFallbackData {
    SyncFallbackData();
    ~SyncFallbackData();
    // Copyable and move-assignable to support Copy-on-Write with Commit.
    SyncFallbackData(const SyncFallbackData& sync_fallback_data);
    SyncFallbackData& operator=(SyncFallbackData&& sync_fallback_data);

    std::string name;
    base::Optional<SkColor> theme_color;
    GURL scope;
    std::vector<WebApplicationIconInfo> icon_infos;
  };
  const SyncFallbackData& sync_fallback_data() const {
    return sync_fallback_data_;
  }

  // Represents the "shortcuts" field in the manifest.
  const std::vector<WebApplicationShortcutsMenuItemInfo>& shortcut_infos()
      const {
    return shortcut_infos_;
  }

  // Represents which shortcuts menu icon sizes we successfully downloaded for
  // each WebAppShortcutsMenuItemInfo.shortcuts_menu_icon_infos.
  const std::vector<std::vector<SquareSizePx>>&
  downloaded_shortcuts_menu_icons_sizes() const {
    return downloaded_shortcuts_menu_icons_sizes_;
  }

  // A Web App can be installed from multiple sources simultaneously. Installs
  // add a source to the app. Uninstalls remove a source from the app.
  void AddSource(Source::Type source);
  void RemoveSource(Source::Type source);
  bool HasAnySources() const;
  bool HasOnlySource(Source::Type source) const;

  bool IsSynced() const;
  bool IsDefaultApp() const;
  bool IsPolicyInstalledApp() const;
  bool IsSystemApp() const;
  bool CanUserUninstallExternalApp() const;
  bool WasInstalledByUser() const;
  // Returns the highest priority source. AppService assumes that every app has
  // just one install source.
  Source::Type GetHighestPrioritySource() const;

  void SetName(const std::string& name);
  void SetDescription(const std::string& description);
  void SetLaunchUrl(const GURL& launch_url);
  void SetScope(const GURL& scope);
  void SetThemeColor(base::Optional<SkColor> theme_color);
  void SetDisplayMode(DisplayMode display_mode);
  void SetUserDisplayMode(DisplayMode user_display_mode);
  void SetUserPageOrdinal(syncer::StringOrdinal page_ordinal);
  void SetUserLaunchOrdinal(syncer::StringOrdinal launch_ordinal);
  void SetWebAppChromeOsData(base::Optional<WebAppChromeOsData> chromeos_data);
  void SetIsLocallyInstalled(bool is_locally_installed);
  void SetIsInSyncInstall(bool is_in_sync_install);
  void SetIconInfos(std::vector<WebApplicationIconInfo> icon_infos);
  // Performs sorting of |sizes| vector. Must be called rarely.
  void SetDownloadedIconSizes(std::vector<SquareSizePx> sizes);
  void SetShortcutInfos(
      std::vector<WebApplicationShortcutsMenuItemInfo> shortcut_infos);
  void SetDownloadedShortcutsMenuIconsSizes(
      std::vector<std::vector<SquareSizePx>> icon_sizes);
  void SetFileHandlers(apps::FileHandlers file_handlers);
  void SetAdditionalSearchTerms(
      std::vector<std::string> additional_search_terms);
  void SetLastLaunchTime(const base::Time& time);
  void SetInstallTime(const base::Time& time);
  void SetSyncFallbackData(SyncFallbackData sync_fallback_data);

 private:
  using Sources = std::bitset<Source::kMaxValue + 1>;
  bool HasAnySpecifiedSourcesAndNoOtherSources(Sources specified_sources) const;

  friend class WebAppDatabase;
  friend bool operator==(const WebApp&, const WebApp&);
  friend std::ostream& operator<<(std::ostream&, const WebApp&);

  AppId app_id_;

  // This set always contains at least one source.
  Sources sources_;

  std::string name_;
  std::string description_;
  GURL launch_url_;
  // TODO(loyso): Implement IsValid() function that verifies that the launch_url
  // is within the scope.
  GURL scope_;
  base::Optional<SkColor> theme_color_;
  DisplayMode display_mode_;
  DisplayMode user_display_mode_;
  syncer::StringOrdinal user_page_ordinal_;
  syncer::StringOrdinal user_launch_ordinal_;
  base::Optional<WebAppChromeOsData> chromeos_data_;
  bool is_locally_installed_ = true;
  bool is_in_sync_install_ = false;
  std::vector<WebApplicationIconInfo> icon_infos_;
  std::vector<SquareSizePx> downloaded_icon_sizes_;
  std::vector<WebApplicationShortcutsMenuItemInfo> shortcut_infos_;
  std::vector<std::vector<SquareSizePx>> downloaded_shortcuts_menu_icons_sizes_;
  apps::FileHandlers file_handlers_;
  std::vector<std::string> additional_search_terms_;
  base::Time last_launch_time_;
  base::Time install_time_;
  SyncFallbackData sync_fallback_data_;
};

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out,
                         const WebApp::SyncFallbackData& sync_fallback_data);
std::ostream& operator<<(std::ostream& out, const WebApp& app);

bool operator==(const WebApp::SyncFallbackData& sync_fallback_data1,
                const WebApp::SyncFallbackData& sync_fallback_data2);
bool operator!=(const WebApp::SyncFallbackData& sync_fallback_data1,
                const WebApp::SyncFallbackData& sync_fallback_data2);

bool operator==(const WebApp& app1, const WebApp& app2);
bool operator!=(const WebApp& app1, const WebApp& app2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
