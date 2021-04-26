// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_MANAGER_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/user_policies.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to fetch user policies from GCPW backends.
class UserPoliciesManager {
 public:
  // Get the user policies manager instance.
  static UserPoliciesManager* Get();

  // Return true if cloud policies feature is enabled.
  bool CloudPoliciesEnabled() const;

  // Fetch the policies for the user from GCPW backend with |sid| using
  // |access_token| for authentication and authorization and saves it in file
  // storage replacing any previously fetched versions.
  virtual HRESULT FetchAndStoreCloudUserPolicies(
      const base::string16& sid,
      const std::string& access_token);

  // Return the elapsed time delta since the last time the policies were
  // successfully fetched for the user with |sid|.
  base::TimeDelta GetTimeDeltaSinceLastPolicyFetch(
      const base::string16& sid) const;

  // Get the URL of GCPW service for HTTP request for fetching user policies.
  GURL GetGcpwServiceUserPoliciesUrl(const base::string16& sid);

  // Retrieves the policies for the user with |sid| from local storage. Returns
  // the default user policy if policy not fetched or on any error.
  virtual bool GetUserPolicies(const base::string16& sid,
                               UserPolicies* user_policies);

  // For testing only return the status of the last policy fetch.
  HRESULT GetLastFetchStatusForTesting() const;

  // For testing manually control if the cloud policies feature is enabled.
  void SetCloudPoliciesEnabledForTesting(bool value);

 protected:
  // Returns the storage used for the instance pointer.
  static UserPoliciesManager** GetInstanceStorage();

  UserPoliciesManager();
  virtual ~UserPoliciesManager();

  HRESULT fetch_status_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_MANAGER_H_
