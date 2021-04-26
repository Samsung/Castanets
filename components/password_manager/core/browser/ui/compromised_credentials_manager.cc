// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_manager.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/containers/flat_set.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace password_manager {

// Extra information about CompromisedCredentials which is required by UI.
struct CredentialMetadata {
  std::vector<autofill::PasswordForm> forms;
  CompromiseTypeFlags type = CompromiseTypeFlags::kNotCompromised;
  base::Time latest_time;
};

namespace {

using CredentialPasswordsMap =
    std::map<CredentialView, CredentialMetadata, PasswordCredentialLess>;

// Transparent comparator that can compare CompromisedCredentials and
// autofill::PasswordForm.
struct CredentialWithoutPasswordLess {
  static std::tuple<const std::string&, const base::string16&>
  CredentialOriginAndUsername(const autofill::PasswordForm& form) {
    return std::tie(form.signon_realm, form.username_value);
  }

  static std::tuple<const std::string&, const base::string16&>
  CredentialOriginAndUsername(const CompromisedCredentials& c) {
    return std::tie(c.signon_realm, c.username);
  }

  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return CredentialOriginAndUsername(lhs) < CredentialOriginAndUsername(rhs);
  }

  using is_transparent = void;
};

CompromiseTypeFlags ConvertCompromiseType(CompromiseType type) {
  switch (type) {
    case CompromiseType::kLeaked:
      return CompromiseTypeFlags::kCredentialLeaked;
    case CompromiseType::kPhished:
      return CompromiseTypeFlags::kCredentialPhished;
  }
  NOTREACHED();
}

// This function takes two lists of compromised credentials and saved passwords
// and joins them, producing a map that contains CredentialWithPassword as keys
// and vector<autofill::PasswordForm> as values with CredentialCompromiseType as
// values.
CredentialPasswordsMap JoinCompromisedCredentialsWithSavedPasswords(
    const std::vector<CompromisedCredentials>& credentials,
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  CredentialPasswordsMap credentials_to_forms;

  // Since a single (signon_realm, username) pair might have multiple
  // corresponding entries in saved_passwords, we are using a multiset and doing
  // look-up via equal_range. In most cases the resulting |range| should have a
  // size of 1, however.
  std::multiset<autofill::PasswordForm, CredentialWithoutPasswordLess>
      password_forms(saved_passwords.begin(), saved_passwords.end());
  for (const auto& credential : credentials) {
    auto range = password_forms.equal_range(credential);
    // Make use of a set to only filter out repeated passwords, if any.
    std::for_each(
        range.first, range.second, [&](const autofill::PasswordForm& form) {
          CredentialView compromised_credential(form);
          auto& credential_to_form =
              credentials_to_forms[compromised_credential];

          // Using |= operator to save in a bit mask both Leaked and Phished.
          credential_to_form.type =
              credential_to_form.type |
              ConvertCompromiseType(credential.compromise_type);

          // Use the latest time. Relevant when the same credential is both
          // phished and compromised.
          credential_to_form.latest_time =
              std::max(credential_to_form.latest_time, credential.create_time);

          // Populate the map. The values are vectors, because it is
          // possible that multiple saved passwords match to the same
          // compromised credential.
          credential_to_form.forms.push_back(form);
        });
  }

  return credentials_to_forms;
}

std::vector<CredentialWithPassword> ExtractCompromisedCredentials(
    const CredentialPasswordsMap& credentials_to_forms) {
  std::vector<CredentialWithPassword> credentials;
  credentials.reserve(credentials_to_forms.size());
  for (const auto& credential_to_forms : credentials_to_forms) {
    CredentialWithPassword credential(credential_to_forms.first);
    credential.compromise_type = credential_to_forms.second.type;
    credential.create_time = credential_to_forms.second.latest_time;
    credentials.push_back(std::move(credential));
  }
  return credentials;
}

}  // namespace

CredentialWithPassword::CredentialWithPassword(const CredentialView& credential)
    : CredentialView(std::move(credential)) {}
CredentialWithPassword::~CredentialWithPassword() = default;
CredentialWithPassword::CredentialWithPassword(
    const CredentialWithPassword& other) = default;

CredentialWithPassword::CredentialWithPassword(CredentialWithPassword&& other) =
    default;
CredentialWithPassword::CredentialWithPassword(
    const CompromisedCredentials& credential) {
  username = credential.username;
  signon_realm = credential.signon_realm;
  create_time = credential.create_time;
  compromise_type = ConvertCompromiseType(credential.compromise_type);
}

CredentialWithPassword& CredentialWithPassword::operator=(
    const CredentialWithPassword& other) = default;
CredentialWithPassword& CredentialWithPassword::operator=(
    CredentialWithPassword&& other) = default;

CompromisedCredentialsManager::CompromisedCredentialsManager(
    scoped_refptr<PasswordStore> store,
    SavedPasswordsPresenter* presenter)
    : store_(std::move(store)), presenter_(presenter) {
  observed_password_store_.Add(store_.get());
  observed_saved_password_presenter_.Add(presenter_);
}

CompromisedCredentialsManager::~CompromisedCredentialsManager() = default;

void CompromisedCredentialsManager::Init() {
  store_->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsManager::SaveCompromisedCredential(
    const LeakCheckCredential& credential) {
  // Iterate over all currently saved credentials and mark those as compromised
  // that have the same canonicalized username and password.
  const base::string16 canonicalized_username =
      CanonicalizeUsername(credential.username());
  for (const autofill::PasswordForm& saved_password :
       presenter_->GetSavedPasswords()) {
    if (saved_password.password_value == credential.password() &&
        CanonicalizeUsername(saved_password.username_value) ==
            canonicalized_username) {
      store_->AddCompromisedCredentials({
          .signon_realm = saved_password.signon_realm,
          .username = saved_password.username_value,
          .create_time = base::Time::Now(),
          .compromise_type = CompromiseType::kLeaked,
      });
    }
  }
}

bool CompromisedCredentialsManager::UpdateCompromisedCredentials(
    const CredentialView& credential,
    const base::StringPiece password) {
  auto it = credentials_to_forms_.find(credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Make sure there are matching password forms. Also erase duplicates if there
  // are any.
  const auto& forms = it->second.forms;
  if (forms.empty())
    return false;

  for (size_t i = 1; i < forms.size(); ++i)
    store_->RemoveLogin(forms[i]);

  // Note: We Invoke EditPassword on the presenter rather than UpdateLogin() on
  // the store, so that observers of the presenter get notified of this event.
  return presenter_->EditPassword(forms[0], base::UTF8ToUTF16(password));
}

bool CompromisedCredentialsManager::RemoveCompromisedCredential(
    const CredentialView& credential) {
  auto it = credentials_to_forms_.find(credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Erase all matching credentials from the store. Return whether any
  // credentials were deleted.
  const auto& saved_passwords = it->second.forms;
  for (const autofill::PasswordForm& saved_password : saved_passwords)
    store_->RemoveLogin(saved_password);

  return !saved_passwords.empty();
}

std::vector<CredentialWithPassword>
CompromisedCredentialsManager::GetCompromisedCredentials() const {
  return ExtractCompromisedCredentials(credentials_to_forms_);
}

SavedPasswordsPresenter::SavedPasswordsView
CompromisedCredentialsManager::GetSavedPasswordsFor(
    const CredentialView& credential) const {
  auto it = credentials_to_forms_.find(credential);
  return it != credentials_to_forms_.end()
             ? it->second.forms
             : SavedPasswordsPresenter::SavedPasswordsView();
}

void CompromisedCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CompromisedCredentialsManager::OnCompromisedCredentialsChanged() {
  // Cancel ongoing requests to the password store and issue a new request.
  cancelable_task_tracker()->TryCancelAll();
  store_->GetAllCompromisedCredentials(this);
}

// Re-computes the list of compromised credentials with passwords after
// obtaining a new list of compromised credentials.
void CompromisedCredentialsManager::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  compromised_credentials_ = std::move(compromised_credentials);
  UpdateCachedDataAndNotifyObservers(presenter_->GetSavedPasswords());
}

// Re-computes the list of compromised credentials with passwords after
// obtaining a new list of saved passwords.
void CompromisedCredentialsManager::OnSavedPasswordsChanged(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  UpdateCachedDataAndNotifyObservers(saved_passwords);
}

void CompromisedCredentialsManager::UpdateCachedDataAndNotifyObservers(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  credentials_to_forms_ = JoinCompromisedCredentialsWithSavedPasswords(
      compromised_credentials_, saved_passwords);
  std::vector<CredentialWithPassword> credentials =
      ExtractCompromisedCredentials(credentials_to_forms_);
  for (auto& observer : observers_) {
    observer.OnCompromisedCredentialsChanged(credentials);
  }
}

}  // namespace password_manager
