// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_source_request.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

namespace {

void ResponseToDownloadCheckResult(
    const enterprise_connectors::ContentAnalysisResponse& response,
    DownloadCheckResult* download_result) {
  bool malware_scan_failure = false;
  bool dlp_scan_failure = false;
  auto malware_action = enterprise_connectors::ContentAnalysisResponse::Result::
      TriggeredRule::ACTION_UNSPECIFIED;
  auto dlp_action = enterprise_connectors::ContentAnalysisResponse::Result::
      TriggeredRule::ACTION_UNSPECIFIED;

  for (const auto& result : response.results()) {
    if (result.tag() == "malware") {
      if (result.status() !=
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
        malware_scan_failure = true;
        continue;
      }
      for (const auto& rule : result.triggered_rules()) {
        if (rule.action() > malware_action)
          malware_action = rule.action();
      }
    }
    if (result.tag() == "dlp") {
      if (result.status() !=
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
        dlp_scan_failure = true;
        continue;
      }
      for (const auto& rule : result.triggered_rules()) {
        if (rule.action() > dlp_action)
          dlp_action = rule.action();
      }
    }
  }

  switch (malware_action) {
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        BLOCK:
      *download_result = DownloadCheckResult::DANGEROUS;
      return;
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        WARN:
      *download_result = DownloadCheckResult::POTENTIALLY_UNWANTED;
      return;
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        REPORT_ONLY:
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        ACTION_UNSPECIFIED:
      break;
  }
  switch (dlp_action) {
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        BLOCK:
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_BLOCK;
      return;
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        WARN:
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_WARNING;
      return;
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        REPORT_ONLY:
    case enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        ACTION_UNSPECIFIED:
      break;
  }

  if (dlp_scan_failure || malware_scan_failure) {
    *download_result = DownloadCheckResult::UNKNOWN;
    return;
  }

  *download_result = DownloadCheckResult::DEEP_SCANNED_SAFE;
}

void ResponseToDownloadCheckResult(const DeepScanningClientResponse& response,
                                   DownloadCheckResult* download_result) {
  if (response.has_malware_scan_verdict()) {
    if (response.malware_scan_verdict().verdict() ==
        MalwareDeepScanningVerdict::MALWARE) {
      *download_result = DownloadCheckResult::DANGEROUS;
      return;
    }

    if (response.malware_scan_verdict().verdict() ==
        MalwareDeepScanningVerdict::UWS) {
      *download_result = DownloadCheckResult::POTENTIALLY_UNWANTED;
      return;
    }
  }

  if (response.has_dlp_scan_verdict() &&
      response.dlp_scan_verdict().status() == DlpDeepScanningVerdict::SUCCESS) {
    bool should_dlp_block = std::any_of(
        response.dlp_scan_verdict().triggered_rules().begin(),
        response.dlp_scan_verdict().triggered_rules().end(),
        [](const DlpDeepScanningVerdict::TriggeredRule& rule) {
          return rule.action() == DlpDeepScanningVerdict::TriggeredRule::BLOCK;
        });
    if (should_dlp_block) {
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_BLOCK;
      return;
    }

    bool should_dlp_warn = std::any_of(
        response.dlp_scan_verdict().triggered_rules().begin(),
        response.dlp_scan_verdict().triggered_rules().end(),
        [](const DlpDeepScanningVerdict::TriggeredRule& rule) {
          return rule.action() == DlpDeepScanningVerdict::TriggeredRule::WARN;
        });
    if (should_dlp_warn) {
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_WARNING;
      return;
    }
  }

  if (response.has_malware_scan_verdict() &&
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::SCAN_FAILURE) {
    *download_result = DownloadCheckResult::UNKNOWN;
    return;
  }

  if (response.has_dlp_scan_verdict() &&
      response.dlp_scan_verdict().status() != DlpDeepScanningVerdict::SUCCESS) {
    *download_result = DownloadCheckResult::UNKNOWN;
    return;
  }

  *download_result = DownloadCheckResult::DEEP_SCANNED_SAFE;
}

bool ShouldUploadForDlpScanByLegacyPolicy() {
  int check_content_compliance = g_browser_process->local_state()->GetInteger(
      prefs::kCheckContentCompliance);
  return (check_content_compliance ==
              CheckContentComplianceValues::CHECK_DOWNLOADS ||
          check_content_compliance ==
              CheckContentComplianceValues::CHECK_UPLOADS_AND_DOWNLOADS);
}

bool ShouldUploadForMalwareScanByLegacyPolicy(download::DownloadItem* item) {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  if (!browser_context)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  int send_files_for_malware_check = profile->GetPrefs()->GetInteger(
      prefs::kSafeBrowsingSendFilesForMalwareCheck);
  return (send_files_for_malware_check ==
              SendFilesForMalwareCheckValues::SEND_DOWNLOADS ||
          send_files_for_malware_check ==
              SendFilesForMalwareCheckValues::SEND_UPLOADS_AND_DOWNLOADS);
}

}  // namespace

/* static */
base::Optional<enterprise_connectors::AnalysisSettings>
DeepScanningRequest::ShouldUploadBinary(download::DownloadItem* item) {
  bool dlp_scan = base::FeatureList::IsEnabled(kContentComplianceEnabled);
  bool malware_scan = base::FeatureList::IsEnabled(kMalwareScanEnabled);
  auto* connectors_manager =
      enterprise_connectors::ConnectorsManager::GetInstance();
  bool use_legacy_policies = !connectors_manager->IsConnectorEnabled(
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);

  // If the settings arent't obtained by the FILE_DOWNLOADED connector, check
  // the legacy DLP and Malware policies.
  if (use_legacy_policies) {
    if (!dlp_scan && !malware_scan)
      return base::nullopt;

    if (dlp_scan)
      dlp_scan = ShouldUploadForDlpScanByLegacyPolicy();
    if (malware_scan)
      malware_scan = ShouldUploadForMalwareScanByLegacyPolicy(item);

    if (!dlp_scan && !malware_scan)
      return base::nullopt;
  }

  // Check that item->GetURL() matches the appropriate URL patterns by getting
  // settings. No settings means no matches were found.
  auto settings = connectors_manager->GetAnalysisSettings(
      item->GetURL(),
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);

  if (!settings.has_value())
    return base::nullopt;

  if (use_legacy_policies) {
    if (!dlp_scan)
      settings.value().tags.erase("dlp");
    if (!malware_scan)
      settings.value().tags.erase("malware");
  }

  if (settings.value().tags.empty())
    return base::nullopt;

  return settings;
}

DeepScanningRequest::DeepScanningRequest(
    download::DownloadItem* item,
    DeepScanTrigger trigger,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* download_service,
    enterprise_connectors::AnalysisSettings settings)
    : item_(item),
      trigger_(trigger),
      callback_(callback),
      download_service_(download_service),
      analysis_settings_(std::move(settings)),
      weak_ptr_factory_(this) {
  item_->AddObserver(this);
}

DeepScanningRequest::~DeepScanningRequest() {
  item_->RemoveObserver(this);
}

void DeepScanningRequest::Start() {
  // Indicate we're now scanning the file.
  callback_.Run(DownloadCheckResult::ASYNC_SCANNING);

  auto request =
      base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseConnectorsEnabled)
          ? std::make_unique<FileSourceRequest>(
                analysis_settings_, item_->GetFullPath(),
                item_->GetTargetFilePath().BaseName(),
                base::BindOnce(&DeepScanningRequest::OnConnectorScanComplete,
                               weak_ptr_factory_.GetWeakPtr()))
          : std::make_unique<FileSourceRequest>(
                analysis_settings_, item_->GetFullPath(),
                item_->GetTargetFilePath().BaseName(),
                base::BindOnce(&DeepScanningRequest::OnLegacyScanComplete,
                               weak_ptr_factory_.GetWeakPtr()));
  request->set_filename(item_->GetTargetFilePath().BaseName().AsUTF8Unsafe());

  std::string raw_digest_sha256 = item_->GetHash();
  request->set_digest(
      base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()));

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));

  if (request->use_legacy_proto())
    PrepareLegacyRequest(request.get(), profile);
  else
    PrepareConnectorRequest(request.get(), profile);

  upload_start_time_ = base::TimeTicks::Now();
  BinaryUploadService* binary_upload_service =
      download_service_->GetBinaryUploadService(profile);
  if (binary_upload_service) {
    binary_upload_service->MaybeUploadForDeepScanning(std::move(request));
  } else {
    if (request->use_legacy_proto()) {
      OnLegacyScanComplete(BinaryUploadService::Result::UNKNOWN,
                           DeepScanningClientResponse());
    } else {
      OnConnectorScanComplete(BinaryUploadService::Result::UNKNOWN,
                              enterprise_connectors::ContentAnalysisResponse());
    }
  }
}

void DeepScanningRequest::PrepareLegacyRequest(
    BinaryUploadService::Request* request,
    Profile* profile) {
  if (trigger_ == DeepScanTrigger::TRIGGER_APP_PROMPT) {
    MalwareDeepScanningClientRequest malware_request;
    malware_request.set_population(
        MalwareDeepScanningClientRequest::POPULATION_TITANIUM);
    request->set_request_malware_scan(std::move(malware_request));
  } else if (trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    policy::DMToken dm_token = GetDMToken(profile);
    request->set_device_token(dm_token.value());

    if (base::FeatureList::IsEnabled(kContentComplianceEnabled) &&
        (analysis_settings_.tags.count("dlp") == 1)) {
      DlpDeepScanningClientRequest dlp_request;
      dlp_request.set_content_source(
          DlpDeepScanningClientRequest::FILE_DOWNLOAD);
      if (item_->GetURL().is_valid())
        dlp_request.set_url(item_->GetURL().spec());
      request->set_request_dlp_scan(std::move(dlp_request));
    }

    if (base::FeatureList::IsEnabled(kMalwareScanEnabled) &&
        (analysis_settings_.tags.count("malware") == 1)) {
      MalwareDeepScanningClientRequest malware_request;
      malware_request.set_population(
          MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
      request->set_request_malware_scan(std::move(malware_request));
    }
  }
}

void DeepScanningRequest::PrepareConnectorRequest(
    BinaryUploadService::Request* request,
    Profile* profile) {
  if (trigger_ == DeepScanTrigger::TRIGGER_POLICY)
    request->set_device_token(GetDMToken(profile).value());

  request->set_analysis_connector(enterprise_connectors::FILE_DOWNLOADED);

  if (item_->GetURL().is_valid())
    request->set_url(item_->GetURL().spec());

  for (const std::string& tag : analysis_settings_.tags)
    request->add_tag(tag);
}

void DeepScanningRequest::OnConnectorScanComplete(
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  OnScanComplete(result, std::move(response));
}

void DeepScanningRequest::OnLegacyScanComplete(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  OnScanComplete(result, std::move(response));
}

template <typename T>
void DeepScanningRequest::OnScanComplete(BinaryUploadService::Result result,
                                         T response) {
  RecordDeepScanMetrics(
      /*access_point=*/DeepScanAccessPoint::DOWNLOAD,
      /*duration=*/base::TimeTicks::Now() - upload_start_time_,
      /*total_size=*/item_->GetTotalBytes(), /*result=*/result,
      /*response=*/response);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (profile && trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    std::string raw_digest_sha256 = item_->GetHash();
    MaybeReportDeepScanningVerdict(
        profile, item_->GetURL(), item_->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        item_->GetMimeType(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
        DeepScanAccessPoint::DOWNLOAD, item_->GetTotalBytes(), result,
        response);
  }

  DownloadCheckResult download_result = DownloadCheckResult::UNKNOWN;
  if (result == BinaryUploadService::Result::SUCCESS) {
    ResponseToDownloadCheckResult(response, &download_result);
  } else if (trigger_ == DeepScanTrigger::TRIGGER_APP_PROMPT &&
             MaybeShowDeepScanFailureModalDialog(
                 base::BindOnce(&DeepScanningRequest::Start,
                                weak_ptr_factory_.GetWeakPtr()),
                 base::BindOnce(&DeepScanningRequest::FinishRequest,
                                weak_ptr_factory_.GetWeakPtr(),
                                DownloadCheckResult::UNKNOWN),
                 base::BindOnce(&DeepScanningRequest::OpenDownload,
                                weak_ptr_factory_.GetWeakPtr()))) {
    return;
  } else if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
    if (analysis_settings_.block_large_files)
      download_result = DownloadCheckResult::BLOCKED_TOO_LARGE;
  } else if (result == BinaryUploadService::Result::FILE_ENCRYPTED) {
    if (analysis_settings_.block_password_protected_files)
      download_result = DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED;
  } else if (result ==
             BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE) {
    if (analysis_settings_.block_unsupported_file_types)
      download_result = DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE;
  }

  FinishRequest(download_result);
}

void DeepScanningRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

void DeepScanningRequest::FinishRequest(DownloadCheckResult result) {
  callback_.Run(result);
  weak_ptr_factory_.InvalidateWeakPtrs();
  item_->RemoveObserver(this);
  download_service_->RequestFinished(this);
}

bool DeepScanningRequest::MaybeShowDeepScanFailureModalDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure open_now_callback) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (!profile)
    return false;

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser)
    return false;

  DeepScanningFailureModalDialog::ShowForWebContents(
      browser->tab_strip_model()->GetActiveWebContents(),
      std::move(accept_callback), std::move(cancel_callback),
      std::move(open_now_callback));
  return true;
}

void DeepScanningRequest::OpenDownload() {
  item_->OpenDownload();
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

}  // namespace safe_browsing
