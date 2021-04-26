// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_METRICS_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_METRICS_H_

#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension.h"

class Profile;

namespace extensions {

// Used to report force-installed extension stats to UMA.
// ExtensionService owns this class and outlives it.
class ForceInstalledMetrics : public ForceInstalledTracker::Observer {
 public:
  ForceInstalledMetrics(ExtensionRegistry* registry,
                        Profile* profile,
                        ForceInstalledTracker* tracker,
                        std::unique_ptr<base::OneShotTimer> timer =
                            std::make_unique<base::OneShotTimer>());

  ForceInstalledMetrics(const ForceInstalledMetrics&) = delete;
  ForceInstalledMetrics& operator=(const ForceInstalledMetrics&) = delete;

  ~ForceInstalledMetrics() override;

  // Note: enum used for UMA. Do NOT reorder or remove entries. Don't forget to
  // update enums.xml (name: SessionType) when adding new
  // entries.
  // Type of session for current user. This enum is required as UserType enum
  // doesn't support new regular users. See user_manager::UserType enum for
  // description of session types other than new and existing regular users.
  enum class SessionType {
    // Session with Regular existing user, which has a user name and password.
    SESSION_TYPE_REGULAR_EXISTING = 0,
    SESSION_TYPE_GUEST = 1,
    // Session with Regular new user, which has a user name and password.
    SESSION_TYPE_REGULAR_NEW = 2,
    SESSION_TYPE_PUBLIC_ACCOUNT = 3,
    SESSION_TYPE_SUPERVISED = 4,
    SESSION_TYPE_KIOSK_APP = 5,
    SESSION_TYPE_CHILD = 6,
    SESSION_TYPE_ARC_KIOSK_APP = 7,
    SESSION_TYPE_ACTIVE_DIRECTORY = 8,
    SESSION_TYPE_WEB_KIOSK_APP = 9,
    // Maximum histogram value.
    kMaxValue = SESSION_TYPE_WEB_KIOSK_APP
  };

  // ForceInstalledTracker::Observer overrides:
  //
  // Calls ReportMetrics method if there is a non-empty list of
  // force-installed extensions, and is responsible for cleanup of
  // observers.
  void OnForceInstalledExtensionsLoaded() override;

  // Reports cache status for the force installed extensions.
  void OnExtensionDownloadCacheStatusRetrieved(
      const ExtensionId& id,
      ExtensionDownloaderDelegate::CacheStatus cache_status) override;

 private:
  // Returns false if the extension status corresponds to a missing extension
  // which is not yet installed or loaded.
  bool IsStatusGood(ForceInstalledTracker::ExtensionStatus status);

  // Returns true only in case of some well-known misconfigurations which are
  // easy to detect. Can return false for misconfigurations which are hard to
  // distinguish with other errors.
  bool IsMisconfiguration(
      const InstallStageTracker::InstallationData& installation_data,
      const ExtensionId& id);

#if defined(OS_CHROMEOS)
  // Returns Session Type in case extension fails to install.
  SessionType GetSessionType();
#endif  // defined(OS_CHROMEOS)

  // Reports disable reasons for the extensions which are installed but not
  // loaded.
  void ReportDisableReason(const ExtensionId& extension_id);

  // If |kInstallationTimeout| report time elapsed for extensions load,
  // otherwise amount of not yet loaded extensions and reasons
  // why they were not installed.
  void ReportMetrics();

  ExtensionRegistry* const registry_;
  Profile* const profile_;
  ForceInstalledTracker* const tracker_;

  // Moment when the class was initialized.
  base::Time start_time_;

  // Tracks whether stats were already reported for the session.
  bool reported_ = false;

  ScopedObserver<ForceInstalledTracker, ForceInstalledTracker::Observer>
      tracker_observer_{this};

  // Tracks installation reporting timeout.
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_METRICS_H_
