// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"

#include "base/feature_list.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {
bool IsInsecureFormAction(const GURL& action_url) {
  if (action_url.SchemeIs(url::kBlobScheme) ||
      action_url.SchemeIs(url::kFileSystemScheme))
    return false;
  return !network::IsOriginPotentiallyTrustworthy(
      url::Origin::Create(action_url));
}
}  // namespace

namespace security_interstitials {

InsecureFormNavigationThrottle::InsecureFormNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory)
    : content::NavigationThrottle(navigation_handle),
      blocking_page_factory_(std::move(blocking_page_factory)) {}

InsecureFormNavigationThrottle::~InsecureFormNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
InsecureFormNavigationThrottle::WillStartRequest() {
  content::NavigationHandle* handle = navigation_handle();
  if (!handle->IsFormSubmission())
    return content::NavigationThrottle::PROCEED;
  content::WebContents* contents = handle->GetWebContents();
  if (!IsInsecureFormAction(handle->GetURL()) ||
      !contents->GetLastCommittedURL().SchemeIs(url::kHttpsScheme)) {
    // Currently we only warn for insecure forms in secure pages.
    return content::NavigationThrottle::PROCEED;
  }

  std::unique_ptr<InsecureFormBlockingPage> blocking_page =
      blocking_page_factory_->CreateInsecureFormBlockingPage(contents,
                                                             handle->GetURL());
  std::string interstitial_html = blocking_page->GetHTMLContents();
  SecurityInterstitialTabHelper::AssociateBlockingPage(
      contents, handle->GetNavigationId(), std::move(blocking_page));
  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html);
}

content::NavigationThrottle::ThrottleCheckResult
InsecureFormNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* InsecureFormNavigationThrottle::GetNameForLogging() {
  return "InsecureFormNavigationThrottle";
}

// static
std::unique_ptr<InsecureFormNavigationThrottle>
InsecureFormNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory) {
  if (!base::FeatureList::IsEnabled(kInsecureFormSubmissionInterstitial))
    return nullptr;
  return std::make_unique<InsecureFormNavigationThrottle>(
      navigation_handle, std::move(blocking_page_factory));
}

}  // namespace security_interstitials
