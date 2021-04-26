// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

class AppRegistrar;
class WebApp;

class WebAppInstallObserver final : public AppRegistrarObserver {
 public:
  explicit WebAppInstallObserver(AppRegistrar* registrar);
  explicit WebAppInstallObserver(Profile* profile);

  // Restricts this observer to only listen for the given |listen_for_app_id|.
  explicit WebAppInstallObserver(AppRegistrar* registrar,
                                 const AppId& listen_for_app_id);
  explicit WebAppInstallObserver(Profile* profile,
                                 const AppId& listen_for_app_id);
  ~WebAppInstallObserver() override;

  AppId AwaitNextInstall();

  using WebAppInstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppInstalledDelegate(WebAppInstalledDelegate delegate);

  using WebAppUninstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppUninstalledDelegate(WebAppUninstalledDelegate delegate);

  using WebAppProfileWillBeDeletedDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppProfileWillBeDeletedDelegate(
      WebAppProfileWillBeDeletedDelegate delegate);

  using WebAppWillBeUpdatedFromSyncDelegate = base::RepeatingCallback<void(
      const std::vector<const WebApp*>& new_apps_state)>;
  void SetWebAppWillBeUpdatedFromSyncDelegate(
      WebAppWillBeUpdatedFromSyncDelegate delegate);

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;

 private:
  base::RunLoop run_loop_;
  AppId app_id_;
  AppId listening_for_app_id_;

  WebAppInstalledDelegate app_installed_delegate_;
  WebAppWillBeUpdatedFromSyncDelegate app_will_be_updated_from_sync_delegate_;
  WebAppUninstalledDelegate app_uninstalled_delegate_;
  WebAppProfileWillBeDeletedDelegate app_profile_will_be_deleted_delegate_;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallObserver);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
