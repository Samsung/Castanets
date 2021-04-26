// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kPassword1[] = "f00b4r";
constexpr char kPassword2[] = "s3cr3t";

using autofill::PasswordForm;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

struct MockCompromisedCredentialsManagerObserver
    : CompromisedCredentialsManager::Observer {
  MOCK_METHOD(void,
              OnCompromisedCredentialsChanged,
              (CompromisedCredentialsManager::CredentialsView),
              (override));
};

using StrictMockCompromisedCredentialsManagerObserver =
    ::testing::StrictMock<MockCompromisedCredentialsManagerObserver>;

CompromisedCredentials MakeCompromised(
    base::StringPiece signon_realm,
    base::StringPiece username,
    CompromiseType type = CompromiseType::kLeaked) {
  return {.signon_realm = std::string(signon_realm),
          .username = base::ASCIIToUTF16(username),
          .compromise_type = type};
}

PasswordForm MakeSavedPassword(base::StringPiece signon_realm,
                               base::StringPiece username,
                               base::StringPiece password,
                               base::StringPiece username_element = "") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.username_element = base::ASCIIToUTF16(username_element);
  return form;
}

LeakCheckCredential MakeLeakCredential(base::StringPiece username,
                                       base::StringPiece password) {
  return LeakCheckCredential(base::ASCIIToUTF16(username),
                             base::ASCIIToUTF16(password));
}

CredentialWithPassword MakeComprmisedCredential(
    PasswordForm form,
    CompromisedCredentials credential) {
  CredentialWithPassword credential_with_password((CredentialView(form)));
  credential_with_password.create_time = credential.create_time;
  credential_with_password.compromise_type =
      credential.compromise_type == CompromiseType::kLeaked
          ? CompromiseTypeFlags::kCredentialLeaked
          : CompromiseTypeFlags::kCredentialPhished;
  return credential_with_password;
}

class CompromisedCredentialsManagerTest : public ::testing::Test {
 protected:
  CompromisedCredentialsManagerTest() { store_->Init(/*prefs=*/nullptr); }

  ~CompromisedCredentialsManagerTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  CompromisedCredentialsManager& provider() { return provider_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
  CompromisedCredentialsManager provider_{store_, &presenter_};
};

}  // namespace

bool operator==(const CredentialWithPassword& lhs,
                const CredentialWithPassword& rhs) {
  return lhs.signon_realm == rhs.signon_realm && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time &&
         lhs.compromise_type == rhs.compromise_type &&
         lhs.password == rhs.password;
}

std::ostream& operator<<(std::ostream& out,
                         const CredentialWithPassword& credential) {
  return out << "{ signon_realm: " << credential.signon_realm
             << ", username: " << credential.username
             << ", create_time: " << credential.create_time
             << ", compromise_type: "
             << static_cast<int>(credential.compromise_type)
             << ", password: " << credential.password << " }";
}

// Tests whether adding and removing an observer works as expected.
TEST_F(CompromisedCredentialsManagerTest,
       NotifyObserversAboutCompromisedCredentialChanges) {
  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1)};

  StrictMockCompromisedCredentialsManagerObserver observer;
  provider().AddObserver(&observer);

  // Adding a compromised credential should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));

  // Adding the exact same credential should not result in a notification, as
  // the database is not actually modified.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();

  // Remove should notify, and observers should be passed an empty list.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged(IsEmpty()));
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());

  // Similarly to repeated add, a repeated remove should not notify either.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));
}

// Tests removing a compromised credentials by compromise type triggers an
// observer works as expected.
TEST_F(CompromisedCredentialsManagerTest,
       NotifyObserversAboutRemovingCompromisedCredentialsByCompromisedType) {
  CompromisedCredentials phished_credentials =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kPhished);
  CompromisedCredentials leaked_credentials =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kLeaked);

  StrictMockCompromisedCredentialsManagerObserver observer;
  provider().AddObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(phished_credentials);
  RunUntilIdle();
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddCompromisedCredentials(leaked_credentials);
  RunUntilIdle();

  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(1);
  store().RemoveCompromisedCredentialsByCompromiseType(
      phished_credentials.signon_realm, phished_credentials.username,
      CompromiseType::kPhished, RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(),
              ElementsAre(leaked_credentials));

  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(1);
  store().RemoveCompromisedCredentialsByCompromiseType(
      leaked_credentials.signon_realm, leaked_credentials.username,
      CompromiseType::kLeaked, RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());
  provider().RemoveObserver(&observer);
}

// Tests whether adding and removing an observer works as expected.
TEST_F(CompromisedCredentialsManagerTest,
       NotifyObserversAboutSavedPasswordsChanges) {
  StrictMockCompromisedCredentialsManagerObserver observer;
  provider().AddObserver(&observer);

  PasswordForm saved_password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  // Adding a saved password should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().AddLogin(saved_password);
  RunUntilIdle();

  // Updating a saved password should notify observers.
  saved_password.password_value = base::ASCIIToUTF16(kPassword2);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().UpdateLogin(saved_password);
  RunUntilIdle();

  // Removing a saved password should notify observers.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged);
  store().RemoveLogin(saved_password);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddLogin(saved_password);
  RunUntilIdle();
}

// Tests that the provider is able to join a single password with a compromised
// credential.
TEST_F(CompromisedCredentialsManagerTest, JoinSingleCredentials) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeComprmisedCredential(password, credential);
  expected.password = password.password_value;
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that the provider is able to join a password with a credential that was
// compromised in multiple ways.
TEST_F(CompromisedCredentialsManagerTest, JoinPhishedAndLeaked) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials leaked =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kLeaked);
  CompromisedCredentials phished =
      MakeCompromised(kExampleCom, kUsername1, CompromiseType::kPhished);

  store().AddLogin(password);
  store().AddCompromisedCredentials(leaked);
  store().AddCompromisedCredentials(phished);
  RunUntilIdle();

  CredentialWithPassword expected = MakeComprmisedCredential(password, leaked);
  expected.password = password.password_value;
  expected.compromise_type = (CompromiseTypeFlags::kCredentialLeaked |
                              CompromiseTypeFlags::kCredentialPhished);

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that the provider reacts whenever the saved passwords or the
// compromised credentials change.
TEST_F(CompromisedCredentialsManagerTest, ReactToChangesInBothTables) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  std::vector<CredentialWithPassword> expected;
  expected.push_back(MakeComprmisedCredential(passwords[0], credentials[0]));
  expected.push_back(MakeComprmisedCredential(passwords[1], credentials[1]));

  store().AddLogin(passwords[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());

  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[0]));

  store().AddLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[0]));

  store().AddCompromisedCredentials(credentials[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAreArray(expected));

  store().RemoveLogin(passwords[0]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected[1]));

  store().RemoveLogin(passwords[1]);
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that the provider is able to join multiple passwords with compromised
// credentials.
TEST_F(CompromisedCredentialsManagerTest, JoinMultipleCredentials) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleCom, kUsername2)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credentials[0]);
  store().AddCompromisedCredentials(credentials[1]);
  RunUntilIdle();

  CredentialWithPassword expected1 =
      MakeComprmisedCredential(passwords[0], credentials[0]);
  CredentialWithPassword expected2 =
      MakeComprmisedCredential(passwords[1], credentials[1]);

  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that joining a compromised credential with saved passwords with a
// different username results in an empty list.
TEST_F(CompromisedCredentialsManagerTest, JoinWitDifferentUsername) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername2, kPassword1),
      MakeSavedPassword(kExampleCom, kUsername2, kPassword2)};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that joining a compromised credential with saved passwords with a
// matching username but different signon_realm results in an empty list.
TEST_F(CompromisedCredentialsManagerTest, JoinWitDifferentSignonRealm) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword1),
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword2)};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in multiple entries
// when the passwords are distinct.
TEST_F(CompromisedCredentialsManagerTest, JoinWithMultipleDistinctPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword2, "element_2")};

  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected1 =
      MakeComprmisedCredential(passwords[0], credential);
  CredentialWithPassword expected2 =
      MakeComprmisedCredential(passwords[1], credential);

  EXPECT_THAT(provider().GetCompromisedCredentials(),
              ElementsAre(expected1, expected2));
}

// Tests that joining a compromised credential with multiple saved passwords for
// the same signon_realm and username combination results in a single entry
// when the passwords are the same.
TEST_F(CompromisedCredentialsManagerTest, JoinWithMultipleRepeatedPasswords) {
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_2")};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeComprmisedCredential(passwords[0], credential);

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Tests that verifies mapping compromised credentials to passwords works
// correctly.
TEST_F(CompromisedCredentialsManagerTest, MapCompromisedPasswordsToPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_1"),
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1, "element_2"),
      MakeSavedPassword(kExampleOrg, kUsername2, kPassword2)};

  std::vector<CompromisedCredentials> credentials = {
      MakeCompromised(kExampleCom, kUsername1),
      MakeCompromised(kExampleOrg, kUsername2)};

  std::vector<CredentialWithPassword> credentuals_with_password;
  credentuals_with_password.push_back(
      MakeComprmisedCredential(passwords[0], credentials[0]));
  credentuals_with_password.push_back(
      MakeComprmisedCredential(passwords[1], credentials[0]));
  credentuals_with_password.push_back(
      MakeComprmisedCredential(passwords[2], credentials[1]));

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddLogin(passwords[2]);
  store().AddCompromisedCredentials(credentials[0]);
  store().AddCompromisedCredentials(credentials[1]);

  RunUntilIdle();
  EXPECT_THAT(provider().GetSavedPasswordsFor(credentuals_with_password[0]),
              ElementsAreArray(store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(credentuals_with_password[1]),
              ElementsAreArray(store().stored_passwords().at(kExampleCom)));

  EXPECT_THAT(provider().GetSavedPasswordsFor(credentuals_with_password[2]),
              ElementsAreArray(store().stored_passwords().at(kExampleOrg)));
}

// Test verifies that saving LeakCheckCredential via provider adds expected
// compromised credential.
TEST_F(CompromisedCredentialsManagerTest, SaveCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  LeakCheckCredential credential = MakeLeakCredential(kUsername1, kPassword1);
  CompromisedCredentials compromised_credential =
      MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password_form);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeComprmisedCredential(password_form, compromised_credential);
  expected.create_time = base::Time::Now();

  provider().SaveCompromisedCredential(credential);
  RunUntilIdle();

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

// Test verifies that editing Compromised Credential via provider change the
// original password form.
TEST_F(CompromisedCredentialsManagerTest, UpdateCompromisedPassword) {
  PasswordForm password_form =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);

  store().AddLogin(password_form);
  store().AddCompromisedCredentials(credential);

  RunUntilIdle();
  CredentialWithPassword expected =
      MakeComprmisedCredential(password_form, credential);

  provider().UpdateCompromisedCredentials(expected, kPassword2);
  RunUntilIdle();
  expected.password = base::UTF8ToUTF16(kPassword2);

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));
}

TEST_F(CompromisedCredentialsManagerTest, RemoveCompromisedCredential) {
  CompromisedCredentials credential = MakeCompromised(kExampleCom, kUsername1);
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);

  store().AddLogin(password);
  store().AddCompromisedCredentials(credential);
  RunUntilIdle();

  CredentialWithPassword expected =
      MakeComprmisedCredential(password, credential);
  expected.password = password.password_value;

  EXPECT_THAT(provider().GetCompromisedCredentials(), ElementsAre(expected));

  EXPECT_TRUE(provider().RemoveCompromisedCredential(expected));
  RunUntilIdle();
  EXPECT_THAT(provider().GetCompromisedCredentials(), IsEmpty());
}

}  // namespace password_manager
