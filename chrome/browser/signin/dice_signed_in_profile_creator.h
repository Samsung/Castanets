// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_
#define CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/gaia/core_account_id.h"

class TokensLoadedCallbackRunner;

// Extracts an account from an existing profile and moves it to a new profile.
class DiceSignedInProfileCreator {
 public:
  // Creates a new profile and moves the account from source_profile to the new
  // profile. The callback is called with the new profile or nullptr in case of
  // failure. The callback is never called synchronously.
  DiceSignedInProfileCreator(Profile* source_profile,
                             CoreAccountId account_id,
                             base::OnceCallback<void(Profile*)> callback);

  ~DiceSignedInProfileCreator();

  DiceSignedInProfileCreator(const DiceSignedInProfileCreator&) = delete;
  DiceSignedInProfileCreator& operator=(const DiceSignedInProfileCreator&) =
      delete;

 private:
  // Callback invoked once a profile is created, so we can transfer the
  // credentials.
  void OnNewProfileCreated(Profile* new_profile, Profile::CreateStatus status);

  // Callback invoked once the token service is ready for the new profile.
  void OnNewProfileTokensLoaded(Profile* new_profile);

  Profile* source_profile_;
  CoreAccountId account_id_;
  base::OnceCallback<void(Profile*)> callback_;
  std::unique_ptr<TokensLoadedCallbackRunner> tokens_loaded_callback_runner_;

  base::WeakPtrFactory<DiceSignedInProfileCreator> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_
