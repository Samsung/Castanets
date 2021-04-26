// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_observer.h"

#include "chrome/browser/web_applications/components/web_app_provider_base.h"

namespace web_app {

WebAppInstallObserver::WebAppInstallObserver(AppRegistrar* registrar) {
  observer_.Add(registrar);
}
WebAppInstallObserver::WebAppInstallObserver(AppRegistrar* registrar,
                                             const AppId& listening_for_id)
    : listening_for_app_id_(listening_for_id) {
  observer_.Add(registrar);
}

WebAppInstallObserver::WebAppInstallObserver(Profile* profile)
    : WebAppInstallObserver(
          &WebAppProviderBase::GetProviderBase(profile)->registrar()) {}

WebAppInstallObserver::WebAppInstallObserver(Profile* profile,
                                             const AppId& listening_for_id)
    : WebAppInstallObserver(
          &WebAppProviderBase::GetProviderBase(profile)->registrar(),
          listening_for_id) {}

WebAppInstallObserver::~WebAppInstallObserver() = default;

AppId WebAppInstallObserver::AwaitNextInstall() {
  run_loop_.Run();
  return std::move(app_id_);
}

void WebAppInstallObserver::SetWebAppInstalledDelegate(
    WebAppInstalledDelegate delegate) {
  app_installed_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppUninstalledDelegate(
    WebAppUninstalledDelegate delegate) {
  app_uninstalled_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppProfileWillBeDeletedDelegate(
    WebAppProfileWillBeDeletedDelegate delegate) {
  app_profile_will_be_deleted_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppWillBeUpdatedFromSyncDelegate(
    WebAppWillBeUpdatedFromSyncDelegate delegate) {
  app_will_be_updated_from_sync_delegate_ = delegate;
}

void WebAppInstallObserver::OnWebAppInstalled(const AppId& app_id) {
  if (!listening_for_app_id_.empty() && app_id != listening_for_app_id_)
    return;

  if (app_installed_delegate_)
    app_installed_delegate_.Run(app_id);

  app_id_ = app_id;
  run_loop_.Quit();
}

void WebAppInstallObserver::OnWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  if (app_will_be_updated_from_sync_delegate_)
    app_will_be_updated_from_sync_delegate_.Run(new_apps_state);
}

void WebAppInstallObserver::OnWebAppUninstalled(const AppId& app_id) {
  if (!listening_for_app_id_.empty() && app_id != listening_for_app_id_)
    return;

  if (app_uninstalled_delegate_)
    app_uninstalled_delegate_.Run(app_id);
}

void WebAppInstallObserver::OnWebAppProfileWillBeDeleted(const AppId& app_id) {
  if (!listening_for_app_id_.empty() && app_id != listening_for_app_id_)
    return;

  if (app_profile_will_be_deleted_delegate_)
    app_profile_will_be_deleted_delegate_.Run(app_id);
}

}  // namespace web_app
