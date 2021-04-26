// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/insecure_form/insecure_form_controller_client.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<security_interstitials::MetricsHelper>
InsecureFormControllerClient::GetMetricsHelper(const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "insecure_form";
  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}

InsecureFormControllerClient::InsecureFormControllerClient(
    content::WebContents* web_contents,
    const GURL& form_target_url)
    : SecurityInterstitialControllerClient(
          web_contents,
          GetMetricsHelper(form_target_url),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL)),
      web_contents_(web_contents) {}

InsecureFormControllerClient::~InsecureFormControllerClient() = default;

void InsecureFormControllerClient::GoBack() {
  SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
}

void InsecureFormControllerClient::Proceed() {
  // TODO(crbug.com/1093955): The simple reload logic means the interstitial is
  // bypassed with any reload (e.g. F5), ideally this shouldn't be the case.

  // We don't check for repost on the proceed reload since the interstitial
  // explains this will submit the form.
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
}
