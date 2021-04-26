// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_reauth_ui.h"

#include <string>

#include "base/check.h"
#include "base/optional.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_util.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "chrome/browser/ui/webui/signin/signin_reauth_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/webui_resources.h"

namespace {

std::string GetAccountImageURL(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // The current version of the reauth only supports the primary account.
  // TODO(crbug.com/1083429): generalize for arbitrary accounts by passing an
  // account id as a method parameter.
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kNotRequired);
  // Sync shouldn't be enabled. Otherwise, the primary account and the first
  // cookie account may diverge.
  DCHECK(!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  base::Optional<AccountInfo> account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);

  return account_info && !account_info->account_image.IsEmpty()
             ? webui::GetBitmapDataUrl(account_info->account_image.AsBitmap())
             : profiles::GetPlaceholderAvatarIconUrl();
}

int GetReauthTitleStringId(signin_metrics::ReauthAccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::ReauthAccessPoint::kUnknown:
    case signin_metrics::ReauthAccessPoint::kAutofillDropdown:
    case signin_metrics::ReauthAccessPoint::kPasswordSettings:
      return IDS_ACCOUNT_PASSWORDS_REAUTH_SHOW_TITLE;
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown:
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu:
    case signin_metrics::ReauthAccessPoint::kPasswordSaveBubble:
    case signin_metrics::ReauthAccessPoint::kPasswordMoveBubble:
      return IDS_ACCOUNT_PASSWORDS_REAUTH_SAVE_TITLE;
  }
}

int GetReauthConfirmButtonLabelStringId(
    signin_metrics::ReauthAccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::ReauthAccessPoint::kUnknown:
    case signin_metrics::ReauthAccessPoint::kAutofillDropdown:
    case signin_metrics::ReauthAccessPoint::kPasswordSettings:
      return IDS_ACCOUNT_PASSWORDS_REAUTH_SHOW_BUTTON_LABEL;
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordDropdown:
    case signin_metrics::ReauthAccessPoint::kGeneratePasswordContextMenu:
    case signin_metrics::ReauthAccessPoint::kPasswordSaveBubble:
    case signin_metrics::ReauthAccessPoint::kPasswordMoveBubble:
      return IDS_ACCOUNT_PASSWORDS_REAUTH_SAVE_BUTTON_LABEL;
  }
}

}  // namespace

SigninReauthUI::SigninReauthUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  signin_metrics::ReauthAccessPoint access_point =
      signin::GetReauthAccessPointForReauthConfirmationURL(
          web_ui->GetWebContents()->GetVisibleURL());

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISigninReauthHost);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->SetDefaultResource(IDR_SIGNIN_REAUTH_HTML);
  source->AddResourcePath("signin_reauth_app.js", IDR_SIGNIN_REAUTH_APP_JS);
  source->AddResourcePath("signin_reauth_browser_proxy.js",
                          IDR_SIGNIN_REAUTH_BROWSER_PROXY_JS);
  source->AddResourcePath("signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS);
  source->AddString("accountImageUrl", GetAccountImageURL(profile));

  // Resources for testing.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);

  // Resources for the account passwords reauth.
  source->AddResourcePath(
      "images/signin_reauth_illustration.svg",
      IDR_SIGNIN_REAUTH_IMAGES_ACCOUNT_PASSWORDS_REAUTH_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "images/signin_reauth_illustration_dark.svg",
      IDR_SIGNIN_REAUTH_IMAGES_ACCOUNT_PASSWORDS_REAUTH_ILLUSTRATION_DARK_SVG);
  source->AddLocalizedString("signinReauthTitle",
                             GetReauthTitleStringId(access_point));
  source->AddLocalizedString("signinReauthDesc",
                             IDS_ACCOUNT_PASSWORDS_REAUTH_DESC);
  source->AddLocalizedString("signinReauthConfirmLabel",
                             GetReauthConfirmButtonLabelStringId(access_point));
  source->AddLocalizedString("signinReauthNextLabel",
                             IDS_ACCOUNT_PASSWORDS_REAUTH_NEXT_BUTTON_LABEL);
  source->AddLocalizedString("signinReauthCloseLabel",
                             IDS_ACCOUNT_PASSWORDS_REAUTH_CLOSE_BUTTON_LABEL);

  content::WebUIDataSource::Add(profile, source);
}

SigninReauthUI::~SigninReauthUI() = default;

void SigninReauthUI::InitializeMessageHandlerWithReauthController(
    SigninReauthViewController* controller) {
  web_ui()->AddMessageHandler(
      std::make_unique<SigninReauthHandler>(controller));
}

void SigninReauthUI::InitializeMessageHandlerWithBrowser(Browser* browser) {}
