// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcuts_menu.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace web_app {

class AppIconManager;
struct ShortcutInfo;

// This class manages creation/update/deletion of OS shortcuts for web
// applications.
//
// TODO(crbug.com/860581): Migrate functions from
// web_app_extension_shortcut.(h|cc) and
// platform_apps/shortcut_manager.(h|cc) to web_app::AppShortcutManager and
// its subclasses.
class AppShortcutManager : public AppRegistrarObserver {
 public:
  explicit AppShortcutManager(Profile* profile);
  ~AppShortcutManager() override;

  void SetSubsystems(AppIconManager* icon_manager, AppRegistrar* registrar);

  void Start();
  void Shutdown();

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppManifestUpdated(const web_app::AppId& app_id,
                               base::StringPiece old_name) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;

  // Tells the AppShortcutManager that no shortcuts should actually be written
  // to the disk.
  void SuppressShortcutsForTesting();

  // virtual for testing.
  virtual bool CanCreateShortcuts() const;
  // virtual for testing.
  virtual void CreateShortcuts(const AppId& app_id,
                               bool add_to_desktop,
                               CreateShortcutsCallback callback);
  virtual void RegisterRunOnOsLogin(const AppId& app_id,
                                    RegisterRunOnOsLoginCallback callback);

  // TODO(crbug.com/1098471): Move this into web_app_shortcuts_menu_win.cc when
  // a callback is integrated into the Shortcuts Menu registration flow.
  using RegisterShortcutsMenuCallback = base::OnceCallback<void(bool success)>;
  // Registers a shortcuts menu for a web app after reading its shortcuts menu
  // icons from disk.
  //
  // TODO(crbug.com/1098471): Consider unifying this method and
  // RegisterShortcutsMenuWithOs() below.
  void ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      const AppId& app_id,
      RegisterShortcutsMenuCallback callback);

  // Registers a shortcuts menu for the web app's icon with the OS.
  //
  // TODO(crbug.com/1098471): Add a callback as part of the Shortcuts Menu
  // registration flow.
  void RegisterShortcutsMenuWithOs(
      const AppId& app_id,
      const std::vector<WebApplicationShortcutsMenuItemInfo>& shortcut_infos,
      const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps);

  void UnregisterShortcutsMenuWithOs(const AppId& app_id);

  // Builds initial ShortcutInfo without |ShortcutInfo::favicon| being read.
  virtual std::unique_ptr<ShortcutInfo> BuildShortcutInfo(
      const AppId& app_id) = 0;

  // The result of a call to GetShortcutInfo.
  using GetShortcutInfoCallback =
      base::OnceCallback<void(std::unique_ptr<ShortcutInfo>)>;
  // Asynchronously gets the information required to create a shortcut for
  // |app_id| including all the icon bitmaps. Returns nullptr if app_id is
  // uninstalled or becomes uninstalled during the asynchronous read of icons.
  virtual void GetShortcutInfoForApp(const AppId& app_id,
                                     GetShortcutInfoCallback callback) = 0;

  using ShortcutCallback = base::OnceCallback<void(const ShortcutInfo*)>;
  static void SetShortcutUpdateCallbackForTesting(ShortcutCallback callback);

 protected:
  void DeleteSharedAppShims(const AppId& app_id);
  void OnShortcutsCreated(const AppId& app_id,
                          CreateShortcutsCallback callback,
                          bool success);

  AppRegistrar* registrar() { return registrar_; }
  Profile* profile() { return profile_; }
  bool suppress_shortcuts_for_testing() const {
    return suppress_shortcuts_for_testing_;
  }

 private:
  void OnShortcutInfoRetrievedCreateShortcuts(
      bool add_to_desktop,
      CreateShortcutsCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutInfoRetrievedRegisterRunOnOsLogin(
      RegisterRunOnOsLoginCallback callback,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutInfoRetrievedUpdateShortcuts(
      base::string16 old_name,
      std::unique_ptr<ShortcutInfo> info);

  void OnShortcutsMenuIconsReadRegisterShortcutsMenu(
      const AppId& app_id,
      RegisterShortcutsMenuCallback callback,
      ShortcutsMenuIconsBitmaps shortcuts_menu_icons_bitmaps);

  ScopedObserver<AppRegistrar, AppRegistrarObserver> app_registrar_observer_{
      this};

  bool suppress_shortcuts_for_testing_ = false;

  AppRegistrar* registrar_ = nullptr;
  AppIconManager* icon_manager_ = nullptr;
  Profile* const profile_;

  base::WeakPtrFactory<AppShortcutManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppShortcutManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_MANAGER_H_
