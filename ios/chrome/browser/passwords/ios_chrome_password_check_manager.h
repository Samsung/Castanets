// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_manager.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// Enum which represents possible states of Password Check on UI.
// It's created based on BulkLeakCheckService::State.
enum class PasswordCheckState {
  kIdle,
  kRunning,
  kSignedOut,
  kOffline,
  kNoPasswords,
  kQuotaLimit,
  kOther,
};

// This class handles the bulk password check feature.
class IOSChromePasswordCheckManager
    : public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::CompromisedCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void PasswordCheckStatusChanged(PasswordCheckState state) {}
    virtual void CompromisedCredentialsChanged(
        password_manager::CompromisedCredentialsManager::CredentialsView
            credentials) {}
  };

  explicit IOSChromePasswordCheckManager(ChromeBrowserState* browser_state);
  ~IOSChromePasswordCheckManager() override;

  // Requests to start a check for compromised passwords.
  void StartPasswordCheck();

  // Returns the current state of the password check.
  PasswordCheckState GetPasswordCheckState() const;

  // The elapsed time since the last full password check was performed.
  base::Time GetLastPasswordCheckTime() const;

  // Obtains all compromised credentials that are present in the password store.
  std::vector<password_manager::CredentialWithPassword>
  GetCompromisedCredentials() const;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords)
      override;

  // password_manager::CompromisedCredentialsProvider::Observer:
  void OnCompromisedCredentialsChanged(
      password_manager::CompromisedCredentialsManager::CredentialsView
          credentials) override;

  // password_manager::BulkLeakCheckServiceInterface::Observer:
  void OnStateChanged(
      password_manager::BulkLeakCheckServiceInterface::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  void NotifyPasswordCheckStatusChanged();

  // Remembers whether a password check is running right now.
  bool is_check_running_ = false;

  ChromeBrowserState* browser_state_ = nullptr;

  // Handle to the password store, powering both |saved_passwords_presenter_|
  // and |compromised_credentials_manager_|.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // Used by |compromised_credentials_manager_| to obtain the list of saved
  // passwords.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Used to obtain the list of compromised credentials.
  password_manager::CompromisedCredentialsManager
      compromised_credentials_manager_;

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_;

  // A scoped observer for |saved_passwords_presenter_|.
  ScopedObserver<password_manager::SavedPasswordsPresenter,
                 password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for |compromised_credentials_manager_|.
  ScopedObserver<password_manager::CompromisedCredentialsManager,
                 password_manager::CompromisedCredentialsManager::Observer>
      observed_compromised_credentials_manager_{this};

  // A scoped observer for the BulkLeakCheckService.
  ScopedObserver<password_manager::BulkLeakCheckServiceInterface,
                 password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_bulk_leak_check_service_{this};

  // Observers to listen to password check changes.
  base::ObserverList<Observer> observers_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_CHECK_MANAGER_H_
