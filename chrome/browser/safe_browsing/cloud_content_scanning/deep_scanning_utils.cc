// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"

namespace safe_browsing {

namespace {

constexpr int kMinBytesPerSecond = 1;
constexpr int kMaxBytesPerSecond = 100 * 1024 * 1024;  // 100 MB/s

std::string MaybeGetUnscannedReason(BinaryUploadService::Result result) {
  std::string unscanned_reason;
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
    case BinaryUploadService::Result::UNAUTHORIZED:
      // Don't report an unscanned file event on these results.
      break;

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      unscanned_reason = "FILE_TOO_LARGE";
      break;
    case BinaryUploadService::Result::TIMEOUT:
    case BinaryUploadService::Result::UNKNOWN:
    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      unscanned_reason = "SERVICE_UNAVAILABLE";
      break;
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      unscanned_reason = "FILE_PASSWORD_PROTECTED";
      break;
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      unscanned_reason = "DLP_SCAN_UNSUPPORTED_FILE_TYPE";
  }

  return unscanned_reason;
}

}  // namespace

ContentAnalysisTrigger::ContentAnalysisTrigger() = default;
ContentAnalysisTrigger::ContentAnalysisTrigger(
    const ContentAnalysisTrigger& other) = default;
ContentAnalysisTrigger::ContentAnalysisTrigger(ContentAnalysisTrigger&& other) =
    default;
ContentAnalysisTrigger::~ContentAnalysisTrigger() = default;
ContentAnalysisTrigger& ContentAnalysisTrigger::operator=(
    const ContentAnalysisTrigger& other) = default;

ContentAnalysisScanResult::ContentAnalysisScanResult() = default;
ContentAnalysisScanResult::ContentAnalysisScanResult(
    const ContentAnalysisScanResult& other) = default;
ContentAnalysisScanResult::ContentAnalysisScanResult(
    ContentAnalysisScanResult&& other) = default;
ContentAnalysisScanResult::~ContentAnalysisScanResult() = default;
ContentAnalysisScanResult& ContentAnalysisScanResult::operator=(
    const ContentAnalysisScanResult& other) = default;

void MaybeReportDeepScanningVerdict(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    BinaryUploadService::Result result,
    const DeepScanningClientResponse& response) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));

  std::string unscanned_reason = MaybeGetUnscannedReason(result);
  if (!unscanned_reason.empty()) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                               mime_type, trigger, access_point,
                               unscanned_reason, content_size);
  }

  if (result != BinaryUploadService::Result::SUCCESS)
    return;

  if (response.has_malware_scan_verdict() &&
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::SCAN_FAILURE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                               mime_type, trigger, access_point,
                               "MALWARE_SCAN_FAILED", content_size);
  }

  if (response.has_dlp_scan_verdict() &&
      response.dlp_scan_verdict().status() != DlpDeepScanningVerdict::SUCCESS) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                               mime_type, trigger, access_point,
                               "DLP_SCAN_FAILED", content_size);
  }

  if (response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::UWS ||
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::MALWARE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnAnalysisConnectorResult(
            url, file_name, download_digest_sha256, mime_type, trigger,
            access_point,
            MalwareVerdictToResult(response.malware_scan_verdict()),
            content_size);
  }

  if (response.dlp_scan_verdict().status() == DlpDeepScanningVerdict::SUCCESS) {
    if (!response.dlp_scan_verdict().triggered_rules().empty()) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
          ->OnAnalysisConnectorResult(
              url, file_name, download_digest_sha256, mime_type, trigger,
              access_point,
              SensitiveDataVerdictToResult(response.dlp_scan_verdict()),
              content_size);
    }
  }
}

void MaybeReportDeepScanningVerdict(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    BinaryUploadService::Result result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));

  std::string unscanned_reason = MaybeGetUnscannedReason(result);
  if (!unscanned_reason.empty()) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                               mime_type, trigger, access_point,
                               unscanned_reason, content_size);
  }

  if (result != BinaryUploadService::Result::SUCCESS)
    return;

  for (auto result : response.results()) {
    if (result.status() !=
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
          ->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                                 mime_type, trigger, access_point,
                                 "ANALYSIS_CONNECTOR_FAILED", content_size);
    } else if (result.triggered_rules_size() > 0) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
          ->OnAnalysisConnectorResult(url, file_name, download_digest_sha256,
                                      mime_type, trigger, access_point,
                                      ContentAnalysisResultToResult(result),
                                      content_size);
    }
  }
}

void ReportAnalysisConnectorWarningBypass(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    const DlpDeepScanningVerdict& verdict) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));
  extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
      ->OnAnalysisConnectorWarningBypassed(
          url, file_name, download_digest_sha256, mime_type, trigger,
          access_point, SensitiveDataVerdictToResult(verdict), content_size);
}

void ReportAnalysisConnectorWarningBypass(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));

  auto results = ContentAnalysisResponseToResults(response);
  for (auto result : results) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnAnalysisConnectorWarningBypassed(
            url, file_name, download_digest_sha256, mime_type, trigger,
            access_point, result, content_size);
  }
}

std::string DeepScanAccessPointToString(DeepScanAccessPoint access_point) {
  switch (access_point) {
    case DeepScanAccessPoint::DOWNLOAD:
      return "Download";
    case DeepScanAccessPoint::UPLOAD:
      return "Upload";
    case DeepScanAccessPoint::DRAG_AND_DROP:
      return "DragAndDrop";
    case DeepScanAccessPoint::PASTE:
      return "Paste";
  }
  NOTREACHED();
  return "";
}

void RecordDeepScanMetrics(
    DeepScanAccessPoint access_point,
    base::TimeDelta duration,
    int64_t total_bytes,
    const BinaryUploadService::Result& result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  // Don't record UMA metrics for this result.
  if (result == BinaryUploadService::Result::UNAUTHORIZED)
    return;
  bool dlp_verdict_success = true;
  bool malware_verdict_success = true;
  for (const auto& result : response.results()) {
    if (result.tag() == "dlp" &&
        result.status() !=
            enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      dlp_verdict_success = false;
    }
    if (result.tag() == "malware" &&
        result.status() !=
            enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      malware_verdict_success = false;
    }
  }

  bool success = dlp_verdict_success && malware_verdict_success;
  std::string result_value = BinaryUploadServiceResultToString(result, success);

  // Update |success| so non-SUCCESS results don't log the bytes/sec metric.
  success &= (result == BinaryUploadService::Result::SUCCESS);

  RecordDeepScanMetrics(access_point, duration, total_bytes, result_value,
                        success);
}

void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const BinaryUploadService::Result& result,
                           const DeepScanningClientResponse& response) {
  // Don't record UMA metrics for this result.
  if (result == BinaryUploadService::Result::UNAUTHORIZED)
    return;
  bool dlp_verdict_success = response.has_dlp_scan_verdict()
                                 ? response.dlp_scan_verdict().status() ==
                                       DlpDeepScanningVerdict::SUCCESS
                                 : true;

  bool malware_verdict_success = true;
  if (response.has_malware_scan_verdict()) {
    switch (response.malware_scan_verdict().verdict()) {
      case MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED:
      case MalwareDeepScanningVerdict::SCAN_FAILURE:
        malware_verdict_success = false;
        break;
      case MalwareDeepScanningVerdict::MALWARE:
      case MalwareDeepScanningVerdict::UWS:
      case MalwareDeepScanningVerdict::CLEAN:
        malware_verdict_success = true;
        break;
    }
  }

  bool success = dlp_verdict_success && malware_verdict_success;
  std::string result_value = BinaryUploadServiceResultToString(result, success);

  // Update |success| so non-SUCCESS results don't log the bytes/sec metric.
  success &= (result == BinaryUploadService::Result::SUCCESS);

  RecordDeepScanMetrics(access_point, duration, total_bytes, result_value,
                        success);
}

void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success) {
  // Don't record metrics if the duration is unusable.
  if (duration.InMilliseconds() == 0)
    return;

  std::string access_point_string = DeepScanAccessPointToString(access_point);
  if (success) {
    base::UmaHistogramCustomCounts(
        "SafeBrowsing.DeepScan." + access_point_string + ".BytesPerSeconds",
        (1000 * total_bytes) / duration.InMilliseconds(),
        /*min=*/kMinBytesPerSecond,
        /*max=*/kMaxBytesPerSecond,
        /*buckets=*/50);
  }

  // The scanning timeout is 5 minutes, so the bucket maximum time is 30 minutes
  // in order to be lenient and avoid having lots of data in the overlow bucket.
  base::UmaHistogramCustomTimes("SafeBrowsing.DeepScan." + access_point_string +
                                    "." + result + ".Duration",
                                duration, base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(30), 50);
  base::UmaHistogramCustomTimes(
      "SafeBrowsing.DeepScan." + access_point_string + ".Duration", duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(30),
      50);
}

std::array<const base::FilePath::CharType*, 24> SupportedDlpFileTypes() {
  // Keep sorted for efficient access.
  static constexpr const std::array<const base::FilePath::CharType*, 24>
      kSupportedDLPFileTypes = {
          FILE_PATH_LITERAL(".7z"),   FILE_PATH_LITERAL(".bz2"),
          FILE_PATH_LITERAL(".bzip"), FILE_PATH_LITERAL(".cab"),
          FILE_PATH_LITERAL(".csv"),  FILE_PATH_LITERAL(".doc"),
          FILE_PATH_LITERAL(".docx"), FILE_PATH_LITERAL(".eps"),
          FILE_PATH_LITERAL(".gz"),   FILE_PATH_LITERAL(".gzip"),
          FILE_PATH_LITERAL(".odt"),  FILE_PATH_LITERAL(".pdf"),
          FILE_PATH_LITERAL(".ppt"),  FILE_PATH_LITERAL(".pptx"),
          FILE_PATH_LITERAL(".ps"),   FILE_PATH_LITERAL(".rar"),
          FILE_PATH_LITERAL(".rtf"),  FILE_PATH_LITERAL(".tar"),
          FILE_PATH_LITERAL(".txt"),  FILE_PATH_LITERAL(".wpd"),
          FILE_PATH_LITERAL(".xls"),  FILE_PATH_LITERAL(".xlsx"),
          FILE_PATH_LITERAL(".xps"),  FILE_PATH_LITERAL(".zip")};
  // TODO: Replace this DCHECK with a static assert once std::is_sorted is
  // constexpr in C++20.
  DCHECK(std::is_sorted(
      kSupportedDLPFileTypes.begin(), kSupportedDLPFileTypes.end(),
      [](const base::FilePath::StringType& a,
         const base::FilePath::StringType& b) { return a.compare(b) < 0; }));

  return kSupportedDLPFileTypes;
}

bool FileTypeSupportedForDlp(const base::FilePath& path) {
  // Accept any file type in the supported list for DLP scans.
  base::FilePath::StringType extension(path.FinalExtension());
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 tolower);

  auto dlp_types = SupportedDlpFileTypes();
  return std::binary_search(dlp_types.begin(), dlp_types.end(), extension);
}

DeepScanningClientResponse SimpleDeepScanningClientResponseForTesting(
    base::Optional<bool> dlp_success,
    base::Optional<bool> malware_success) {
  DeepScanningClientResponse response;

  if (dlp_success.has_value()) {
    response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
    if (!dlp_success.value()) {
      DlpDeepScanningVerdict::TriggeredRule* rule =
          response.mutable_dlp_scan_verdict()->add_triggered_rules();
      rule->set_rule_name("rule");
      rule->set_action(DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    }
  }

  if (malware_success.has_value()) {
    if (malware_success.value()) {
      response.mutable_malware_scan_verdict()->set_verdict(
          MalwareDeepScanningVerdict::CLEAN);
    } else {
      response.mutable_malware_scan_verdict()->set_verdict(
          MalwareDeepScanningVerdict::MALWARE);
    }
  }

  return response;
}

std::string BinaryUploadServiceResultToString(
    const BinaryUploadService::Result& result,
    bool success) {
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
      if (success)
        return "Success";
      else
        return "FailedToGetVerdict";
    case BinaryUploadService::Result::UPLOAD_FAILURE:
      return "UploadFailure";
    case BinaryUploadService::Result::TIMEOUT:
      return "Timeout";
    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return "FileTooLarge";
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      return "FailedToGetToken";
    case BinaryUploadService::Result::UNKNOWN:
      return "Unknown";
    case BinaryUploadService::Result::UNAUTHORIZED:
      return "";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FileEncrypted";
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return "DlpScanUnsupportedFileType";
  }
}

ContentAnalysisScanResult SensitiveDataVerdictToResult(
    const safe_browsing::DlpDeepScanningVerdict& verdict) {
  ContentAnalysisScanResult result;
  result.tag = "dlp";
  result.status = verdict.status();
  for (auto rule : verdict.triggered_rules()) {
    ContentAnalysisTrigger trigger;
    trigger.action = rule.action();
    trigger.id =
        rule.has_rule_id() ? base::NumberToString(rule.rule_id()) : "0";
    trigger.name = rule.rule_name();
    result.triggers.push_back(std::move(trigger));
  }
  return result;
}

ContentAnalysisScanResult ContentAnalysisResultToResult(
    const enterprise_connectors::ContentAnalysisResponse::Result& result) {
  ContentAnalysisScanResult result2;
  result2.tag = result.tag();
  result2.status = result.status();

  for (auto rule : result.triggered_rules()) {
    ContentAnalysisTrigger trigger;
    trigger.action = rule.action();
    trigger.id = rule.rule_id();
    trigger.name = rule.rule_name();
    result2.triggers.push_back(std::move(trigger));
  }

  return result2;
}

ContentAnalysisScanResult MalwareVerdictToResult(
    const safe_browsing::MalwareDeepScanningVerdict& verdict) {
  DCHECK_NE(MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED, verdict.verdict());
  DCHECK_NE(MalwareDeepScanningVerdict::SCAN_FAILURE, verdict.verdict());

  ContentAnalysisScanResult result;
  result.tag = "malware";
  result.status =
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS;

  if (verdict.verdict() != MalwareDeepScanningVerdict::CLEAN) {
    ContentAnalysisTrigger trigger;
    switch (verdict.verdict()) {
      case MalwareDeepScanningVerdict::UWS:
        trigger.action = enterprise_connectors::ContentAnalysisResponse::
            Result::TriggeredRule::BLOCK;
        trigger.name = "UWS";
        break;
      case MalwareDeepScanningVerdict::MALWARE:
        trigger.action = enterprise_connectors::ContentAnalysisResponse::
            Result::TriggeredRule::BLOCK;
        trigger.name = "MALWARE";
        break;
      default:
        NOTREACHED();
        break;
    }
    result.triggers.push_back(std::move(trigger));
  }
  return result;
}

std::vector<ContentAnalysisScanResult> ContentAnalysisResponseToResults(
    const enterprise_connectors::ContentAnalysisResponse& response) {
  std::vector<ContentAnalysisScanResult> results;
  for (auto result : response.results()) {
    results.push_back(ContentAnalysisResultToResult(result));
  }
  return results;
}

}  // namespace safe_browsing
