// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"

namespace updater {

const char kPrefQualified[] = "qualified";
const char kPrefSwapping[] = "swapping";
const char kPrefActiveVersion[] = "active_version";

UpdaterPrefs::UpdaterPrefs(std::unique_ptr<ScopedPrefsLock> lock,
                           std::unique_ptr<PrefService> prefs)
    : lock_(std::move(lock)), prefs_(std::move(prefs)) {}

UpdaterPrefs::~UpdaterPrefs() = default;

PrefService* UpdaterPrefs::GetPrefService() const {
  return prefs_.get();
}

std::unique_ptr<UpdaterPrefs> CreateGlobalPrefs() {
  std::unique_ptr<ScopedPrefsLock> lock =
      AcquireGlobalPrefsLock(base::TimeDelta::FromMinutes(2));
  if (!lock)
    return nullptr;

  base::FilePath global_prefs_dir;
  if (!GetBaseDirectory(&global_prefs_dir))
    return nullptr;

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      global_prefs_dir.Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefSwapping, false);
  pref_registry->RegisterStringPref(kPrefActiveVersion, "0");

  return std::make_unique<UpdaterPrefs>(
      std::move(lock), pref_service_factory.Create(pref_registry));
}

std::unique_ptr<UpdaterPrefs> CreateLocalPrefs() {
  base::FilePath local_prefs_dir;
  if (!GetVersionedDirectory(&local_prefs_dir))
    return nullptr;

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      local_prefs_dir.Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefQualified, false);

  return std::make_unique<UpdaterPrefs>(
      nullptr, pref_service_factory.Create(pref_registry));
}

void PrefsCommitPendingWrites(PrefService* pref_service) {
  // Waits in the run loop until pending writes complete.
  base::RunLoop runloop;
  pref_service->CommitPendingWrite(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      runloop.QuitWhenIdleClosure()));
  runloop.Run();
}

}  // namespace updater
