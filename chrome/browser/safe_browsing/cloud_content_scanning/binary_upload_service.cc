// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/strings.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"

namespace safe_browsing {
namespace {

const int kScanningTimeoutSeconds = 5 * 60;  // 5 minutes

const char kSbEnterpriseUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/scan";

const char kSbAppUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/app";

bool IsAdvancedProtectionRequest(const BinaryUploadService::Request& request) {
  if (request.use_legacy_proto()) {
    return !request.deep_scanning_request().has_dlp_scan_request() &&
           request.deep_scanning_request().has_malware_scan_request() &&
           request.deep_scanning_request()
                   .malware_scan_request()
                   .population() ==
               MalwareDeepScanningClientRequest::POPULATION_TITANIUM;
  } else {
    return request.device_token().empty();
  }
}

std::string ResultToString(BinaryUploadService::Result result) {
  switch (result) {
    case BinaryUploadService::Result::UNKNOWN:
      return "UNKNOWN";
    case BinaryUploadService::Result::SUCCESS:
      return "SUCCESS";
    case BinaryUploadService::Result::UPLOAD_FAILURE:
      return "UPLOAD_FAILURE";
    case BinaryUploadService::Result::TIMEOUT:
      return "TIMEOUT";
    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return "FILE_TOO_LARGE";
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      return "FAILED_TO_GET_TOKEN";
    case BinaryUploadService::Result::UNAUTHORIZED:
      return "UNAUTHORIZED";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FILE_ENCRYPTED";
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return "DLP_SCAN_UNSUPPORTED_FILE_TYPE";
  }
}

}  // namespace

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(BinaryFCMService::Create(profile)),
      profile_(profile),
      weakptr_factory_(this) {}

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    std::unique_ptr<BinaryFCMService> binary_fcm_service)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(std::move(binary_fcm_service)),
      profile_(profile),
      weakptr_factory_(this) {}

BinaryUploadService::~BinaryUploadService() {}

void BinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsAdvancedProtectionRequest(*request)) {
    MaybeUploadForDeepScanningCallback(
        std::move(request),
        /*authorized=*/safe_browsing::AdvancedProtectionStatusManagerFactory::
            GetForProfile(profile_)
                ->IsUnderAdvancedProtection());
    return;
  }

  if (!can_upload_enterprise_data_.has_value()) {
    // Get the URL first since |request| is about to move.
    GURL url = request->GetUrlWithParams();
    IsAuthorized(
        std::move(url),
        base::BindOnce(&BinaryUploadService::MaybeUploadForDeepScanningCallback,
                       weakptr_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  MaybeUploadForDeepScanningCallback(std::move(request),
                                     can_upload_enterprise_data_.value());
}

void BinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<BinaryUploadService::Request> request,
    bool authorized) {
  // Ignore the request if the browser cannot upload data.
  if (!authorized) {
    // TODO(crbug/1028133): Add extra logic to handle UX for non-authorized
    // users.
    request->FinishRequest(Result::UNAUTHORIZED);
    return;
  }
  UploadForDeepScanning(std::move(request));
}

void BinaryUploadService::UploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Request* raw_request = request.get();
  active_requests_[raw_request] = std::move(request);
  start_times_[raw_request] = base::TimeTicks::Now();

  std::string token = base::RandBytesAsString(128);
  token = base::HexEncode(token.data(), token.size());
  active_tokens_[raw_request] = token;
  raw_request->set_request_token(token);

  if (!binary_fcm_service_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BinaryUploadService::FinishRequest,
                                  weakptr_factory_.GetWeakPtr(), raw_request,
                                  Result::FAILED_TO_GET_TOKEN));
    return;
  }

  if (raw_request->use_legacy_proto()) {
    binary_fcm_service_->SetCallbackForToken(
        token, base::BindRepeating(&BinaryUploadService::OnGetLegacyResponse,
                                   weakptr_factory_.GetWeakPtr(), raw_request));
  } else {
    binary_fcm_service_->SetCallbackForToken(
        token, base::BindRepeating(&BinaryUploadService::OnGetConnectorResponse,
                                   weakptr_factory_.GetWeakPtr(), raw_request));
  }
  binary_fcm_service_->GetInstanceID(
      base::BindOnce(&BinaryUploadService::OnGetInstanceID,
                     weakptr_factory_.GetWeakPtr(), raw_request));
  active_timers_[raw_request] = std::make_unique<base::OneShotTimer>();
  active_timers_[raw_request]->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kScanningTimeoutSeconds),
      base::BindOnce(&BinaryUploadService::OnTimeout,
                     weakptr_factory_.GetWeakPtr(), raw_request));
}

void BinaryUploadService::OnGetInstanceID(Request* request,
                                          const std::string& instance_id) {
  if (!IsActive(request))
    return;

  if (instance_id == BinaryFCMService::kInvalidId) {
    FinishRequest(request, Result::FAILED_TO_GET_TOKEN);
    return;
  }

  base::UmaHistogramCustomTimes(
      "SafeBrowsingBinaryUploadRequest.TimeToGetFCMToken",
      base::TimeTicks::Now() - start_times_[request],
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(6),
      50);

  request->set_fcm_token(instance_id);
  request->GetRequestData(base::BindOnce(&BinaryUploadService::OnGetRequestData,
                                         weakptr_factory_.GetWeakPtr(),
                                         request));
}

void BinaryUploadService::OnGetRequestData(Request* request,
                                           Result result,
                                           const Request::Data& data) {
  if (!IsActive(request))
    return;

  if (result != Result::SUCCESS) {
    FinishRequest(request, result);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_binary_upload", R"(
        semantics {
          sender: "Safe Browsing Download Protection"
          description:
            "For users with the enterprise policy "
            "SendFilesForMalwareCheck set, when a file is "
            "downloaded, Chrome will upload that file to Safe Browsing for "
            "detailed scanning."
          trigger:
            "The browser will upload the file to Google when "
            "the user downloads a file, and the enterprise policy "
            "SendFilesForMalwareCheck is set."
          data:
            "The downloaded file."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            SendFilesForMalwareCheck {
              SendFilesForMalwareCheck: 0
            }
          }
          chrome_policy {
            SendFilesForMalwareCheck {
              SendFilesForMalwareCheck: 1
            }
          }
        }
        comments: "Setting SendFilesForMalwareCheck to 0 (Do not scan "
          "downloads) or 1 (Forbid the scanning of downloads) will disable "
          "this feature"
        )");

  std::string metadata;
  request->SerializeToString(&metadata);
  base::Base64Encode(metadata, &metadata);

  GURL url = request->GetUrlWithParams();
  if (!url.is_valid())
    url = GetUploadUrl(IsAdvancedProtectionRequest(*request));
  auto upload_request = MultipartUploadRequest::Create(
      url_loader_factory_, std::move(url), metadata, data.contents,
      traffic_annotation,
      base::BindOnce(&BinaryUploadService::OnUploadComplete,
                     weakptr_factory_.GetWeakPtr(), request));

  if (request->use_legacy_proto()) {
    WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
        request->deep_scanning_request());
  } else {
    WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
        request->content_analysis_request());
  }

  // |request| might have been deleted by the call to Start() in tests, so don't
  // dereference it afterwards.
  upload_request->Start();
  active_uploads_[request] = std::move(upload_request);
}

void BinaryUploadService::OnUploadComplete(Request* request,
                                           bool success,
                                           const std::string& response_data) {
  if (!IsActive(request))
    return;

  if (!success) {
    FinishRequest(request, Result::UPLOAD_FAILURE);
    return;
  }

  if (request->use_legacy_proto()) {
    DeepScanningClientResponse response;
    if (!response.ParseFromString(response_data)) {
      FinishRequest(request, Result::UPLOAD_FAILURE);
      return;
    }

    active_uploads_.erase(request);

    // Synchronous scans can return results in the initial response proto, so
    // check for those.
    OnGetLegacyResponse(request, response);
  } else {
    enterprise_connectors::ContentAnalysisResponse response;
    if (!response.ParseFromString(response_data)) {
      FinishRequest(request, Result::UPLOAD_FAILURE);
      return;
    }

    active_uploads_.erase(request);

    // Synchronous scans can return results in the initial response proto, so
    // check for those.
    OnGetConnectorResponse(request, response);
  }
}

void BinaryUploadService::OnGetConnectorResponse(
    Request* request,
    enterprise_connectors::ContentAnalysisResponse response) {
  if (!IsActive(request))
    return;

  for (const auto& result : response.results()) {
    if (result.has_tag() && !result.tag().empty()) {
      VLOG(1) << "Request " << request->request_token()
              << " finished scanning tag <" << result.tag() << ">";
      *received_connector_responses_[request].add_results() = result;
    }
  }

  MaybeFinishConnectorRequest(request);
}

void BinaryUploadService::OnGetLegacyResponse(
    Request* request,
    DeepScanningClientResponse response) {
  if (!IsActive(request))
    return;

  if (response.has_dlp_scan_verdict()) {
    VLOG(1) << "Request " << request->request_token()
            << " finished DLP scanning";
    received_dlp_verdicts_[request].reset(response.release_dlp_scan_verdict());
  }

  if (response.has_malware_scan_verdict()) {
    VLOG(1) << "Request " << request->request_token()
            << " finished malware scanning";
    received_malware_verdicts_[request].reset(
        response.release_malware_scan_verdict());
  }

  MaybeFinishLegacyRequest(request);
}

void BinaryUploadService::MaybeFinishConnectorRequest(Request* request) {
  for (const std::string& tag : request->content_analysis_request().tags()) {
    const auto& results = received_connector_responses_[request].results();
    if (std::none_of(
            results.begin(), results.end(),
            [&tag](const auto& result) { return result.tag() == tag; })) {
      VLOG(1) << "Request " << request->request_token() << " is waiting for <"
              << tag << "> scanning to complete.";
      return;
    }
  }

  // It's OK to move here since the map entry is about to be removed.
  enterprise_connectors::ContentAnalysisResponse response =
      std::move(received_connector_responses_[request]);
  response.set_request_token(request->request_token());
  FinishConnectorRequest(request, Result::SUCCESS, std::move(response));
}

void BinaryUploadService::MaybeFinishLegacyRequest(Request* request) {
  bool requested_dlp_scan_response =
      request->deep_scanning_request().has_dlp_scan_request();
  auto received_dlp_response = received_dlp_verdicts_.find(request);
  if (requested_dlp_scan_response &&
      received_dlp_response == received_dlp_verdicts_.end()) {
    VLOG(1) << "Request " << request->request_token()
            << " is waiting for DLP scanning to complete.";
    return;
  }

  bool requested_malware_scan_response =
      request->deep_scanning_request().has_malware_scan_request();
  auto received_malware_response = received_malware_verdicts_.find(request);
  if (requested_malware_scan_response &&
      received_malware_response == received_malware_verdicts_.end()) {
    VLOG(1) << "Request " << request->request_token()
            << " is waiting for malware scanning to complete.";
    return;
  }

  DeepScanningClientResponse response;
  response.set_token(request->request_token());
  if (requested_dlp_scan_response) {
    // Transfers ownership of the DLP response to |response|.
    response.set_allocated_dlp_scan_verdict(
        received_dlp_response->second.release());
  }

  if (requested_malware_scan_response) {
    // Transfers ownership of the malware response to |response|.
    response.set_allocated_malware_scan_verdict(
        received_malware_response->second.release());
  }

  FinishLegacyRequest(request, Result::SUCCESS, std::move(response));
}

void BinaryUploadService::OnTimeout(Request* request) {
  if (IsActive(request))
    FinishRequest(request, Result::TIMEOUT);
}

void BinaryUploadService::FinishRequest(Request* request, Result result) {
  if (request->use_legacy_proto()) {
    FinishLegacyRequest(request, result, DeepScanningClientResponse());
  } else {
    FinishConnectorRequest(request, result,
                           enterprise_connectors::ContentAnalysisResponse());
  }
}

void BinaryUploadService::FinishConnectorRequest(
    Request* request,
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordRequestMetrics(request, result, response);

  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->content_analysis_request());
  WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      active_tokens_[request], ResultToString(result), response);

  std::string instance_id = request->fcm_notification_token();
  request->FinishConnectorRequest(result, response);
  FinishRequestCleanup(request, instance_id);
}

void BinaryUploadService::FinishLegacyRequest(
    Request* request,
    Result result,
    DeepScanningClientResponse response) {
  RecordRequestMetrics(request, result, response);

  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->deep_scanning_request());
  WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      active_tokens_[request], ResultToString(result), response);

  std::string instance_id = request->fcm_notification_token();
  request->FinishLegacyRequest(result, response);
  FinishRequestCleanup(request, instance_id);
}

void BinaryUploadService::FinishRequestCleanup(Request* request,
                                               const std::string& instance_id) {
  active_requests_.erase(request);
  active_timers_.erase(request);
  active_uploads_.erase(request);
  received_malware_verdicts_.erase(request);
  received_dlp_verdicts_.erase(request);
  received_connector_responses_.erase(request);

  auto token_it = active_tokens_.find(request);
  DCHECK(token_it != active_tokens_.end());
  if (binary_fcm_service_) {
    binary_fcm_service_->ClearCallbackForToken(token_it->second);

    // The BinaryFCMService will handle all recoverable errors. In case of
    // unrecoverable error, there's nothing we can do here.
    binary_fcm_service_->UnregisterInstanceID(
        instance_id,
        base::BindOnce(&BinaryUploadService::InstanceIDUnregisteredCallback,
                       weakptr_factory_.GetWeakPtr()));
  } else {
    // |binary_fcm_service_| can be null in tests, but
    // InstanceIDUnregisteredCallback should be called anyway so the requests
    // waiting on authentication can complete.
    InstanceIDUnregisteredCallback(true);
  }

  active_tokens_.erase(token_it);
}

void BinaryUploadService::InstanceIDUnregisteredCallback(bool) {
  // Calling RunAuthorizationCallbacks after the instance ID of the initial
  // authentication is unregistered avoids registration/unregistration conflicts
  // with normal requests.
  if (!authorization_callbacks_.empty() &&
      can_upload_enterprise_data_.has_value()) {
    RunAuthorizationCallbacks();
  }
}

void BinaryUploadService::RecordRequestMetrics(Request* request,
                                               Result result) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                base::TimeTicks::Now() - start_times_[request],
                                base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(6), 50);
}

void BinaryUploadService::RecordRequestMetrics(
    Request* request,
    Result result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  RecordRequestMetrics(request, result);
  for (const auto& result : response.results()) {
    if (result.tag() == "malware") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.MalwareResult",
          result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
    if (result.tag() == "dlp") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.DlpResult",
          result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
  }
}

void BinaryUploadService::RecordRequestMetrics(
    Request* request,
    Result result,
    const DeepScanningClientResponse& response) {
  RecordRequestMetrics(request, result);
  if (response.has_malware_scan_verdict()) {
    base::UmaHistogramBoolean("SafeBrowsingBinaryUploadRequest.MalwareResult",
                              response.malware_scan_verdict().verdict() !=
                                  MalwareDeepScanningVerdict::SCAN_FAILURE);
    MalwareDeepScanningVerdict::Verdict verdict_count =
        static_cast<MalwareDeepScanningVerdict::Verdict>(
            MalwareDeepScanningVerdict_Verdict_Verdict_ARRAYSIZE);
    base::UmaHistogramEnumeration(
        IsAdvancedProtectionRequest(*request)
            ? "SafeBrowsingBinaryUploadRequest.AdvancedProtectionScanVerdict"
            : "SafeBrowsingBinaryUploadRequest.MalwareScanVerdict",
        response.malware_scan_verdict().verdict(), verdict_count);
  }

  if (response.has_dlp_scan_verdict()) {
    base::UmaHistogramBoolean("SafeBrowsingBinaryUploadRequest.DlpResult",
                              response.dlp_scan_verdict().status() ==
                                  DlpDeepScanningVerdict::SUCCESS);
  }
}

BinaryUploadService::Request::Data::Data() = default;

BinaryUploadService::Request::Request(Callback callback, GURL url)
    : use_legacy_proto_(true), callback_(std::move(callback)), url_(url) {}

BinaryUploadService::Request::Request(ContentAnalysisCallback callback,
                                      GURL url)
    : use_legacy_proto_(false),
      content_analysis_callback_(std::move(callback)),
      url_(url) {}

BinaryUploadService::Request::~Request() = default;

void BinaryUploadService::Request::set_request_dlp_scan(
    DlpDeepScanningClientRequest dlp_request) {
  DCHECK(use_legacy_proto_);
  *deep_scanning_request_.mutable_dlp_scan_request() = std::move(dlp_request);
}

void BinaryUploadService::Request::set_request_malware_scan(
    MalwareDeepScanningClientRequest malware_request) {
  DCHECK(use_legacy_proto_);
  *deep_scanning_request_.mutable_malware_scan_request() =
      std::move(malware_request);
}

void BinaryUploadService::Request::set_fcm_token(const std::string& token) {
  if (use_legacy_proto_)
    deep_scanning_request_.set_fcm_notification_token(token);
  else
    content_analysis_request_.set_fcm_notification_token(token);
}

void BinaryUploadService::Request::set_device_token(const std::string& token) {
  if (use_legacy_proto_)
    deep_scanning_request_.set_dm_token(token);
  else
    content_analysis_request_.set_device_token(token);
}

void BinaryUploadService::Request::set_request_token(const std::string& token) {
  if (use_legacy_proto_)
    deep_scanning_request_.set_request_token(token);
  else
    content_analysis_request_.set_request_token(token);
}

void BinaryUploadService::Request::set_filename(const std::string& filename) {
  if (use_legacy_proto_)
    deep_scanning_request_.set_filename(filename);
  else
    content_analysis_request_.mutable_request_data()->set_filename(filename);
}

void BinaryUploadService::Request::set_digest(const std::string& digest) {
  if (use_legacy_proto_)
    deep_scanning_request_.set_digest(digest);
  else
    content_analysis_request_.mutable_request_data()->set_digest(digest);
}

void BinaryUploadService::Request::clear_dlp_scan_request() {
  if (use_legacy_proto()) {
    deep_scanning_request_.clear_dlp_scan_request();
  } else {
    auto* tags = content_analysis_request_.mutable_tags();
    auto it = std::find(tags->begin(), tags->end(), "dlp");
    if (it != tags->end())
      tags->erase(it);
  }
}

void BinaryUploadService::Request::set_analysis_connector(
    enterprise_connectors::AnalysisConnector connector) {
  DCHECK(!use_legacy_proto_);
  content_analysis_request_.set_analysis_connector(connector);
}

void BinaryUploadService::Request::set_url(const std::string& url) {
  DCHECK(!use_legacy_proto_);
  content_analysis_request_.mutable_request_data()->set_url(url);
}

void BinaryUploadService::Request::set_csd(ClientDownloadRequest csd) {
  DCHECK(!use_legacy_proto_);
  *content_analysis_request_.mutable_request_data()->mutable_csd() =
      std::move(csd);
}

void BinaryUploadService::Request::add_tag(const std::string& tag) {
  DCHECK(!use_legacy_proto_);
  content_analysis_request_.add_tags(tag);
}

const std::string& BinaryUploadService::Request::device_token() const {
  if (use_legacy_proto_)
    return deep_scanning_request_.dm_token();
  else
    return content_analysis_request_.device_token();
}

const std::string& BinaryUploadService::Request::request_token() const {
  if (use_legacy_proto_)
    return deep_scanning_request_.request_token();
  else
    return content_analysis_request_.request_token();
}

const std::string& BinaryUploadService::Request::fcm_notification_token()
    const {
  if (use_legacy_proto_)
    return deep_scanning_request_.fcm_notification_token();
  else
    return content_analysis_request_.fcm_notification_token();
}

const std::string& BinaryUploadService::Request::filename() const {
  if (use_legacy_proto_)
    return deep_scanning_request_.filename();
  else
    return content_analysis_request_.request_data().filename();
}

const std::string& BinaryUploadService::Request::digest() const {
  if (use_legacy_proto_)
    return deep_scanning_request_.digest();
  else
    return content_analysis_request_.request_data().digest();
}

void BinaryUploadService::Request::FinishRequest(Result result) {
  if (use_legacy_proto_) {
    std::move(callback_).Run(result, DeepScanningClientResponse());
  } else {
    std::move(content_analysis_callback_)
        .Run(result, enterprise_connectors::ContentAnalysisResponse());
  }
}

void BinaryUploadService::Request::FinishConnectorRequest(
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  std::move(content_analysis_callback_).Run(result, response);
}

void BinaryUploadService::Request::FinishLegacyRequest(
    Result result,
    DeepScanningClientResponse response) {
  std::move(callback_).Run(result, response);
}

void BinaryUploadService::Request::SerializeToString(
    std::string* destination) const {
  if (use_legacy_proto_)
    deep_scanning_request_.SerializeToString(destination);
  else
    content_analysis_request_.SerializeToString(destination);
}

GURL BinaryUploadService::Request::GetUrlWithParams() const {
  if (use_legacy_proto_)
    return url_;

  GURL url(url_);

  url = net::AppendQueryParameter(url, enterprise::kUrlParamDeviceToken,
                                  device_token());

  std::string connector;
  switch (content_analysis_request_.analysis_connector()) {
    case enterprise_connectors::FILE_ATTACHED:
      connector = "OnFileAttached";
      break;
    case enterprise_connectors::FILE_DOWNLOADED:
      connector = "OnFileDownloaded";
      break;
    case enterprise_connectors::BULK_DATA_ENTRY:
      connector = "OnBulkDataEntry";
      break;
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      break;
  }
  if (!connector.empty()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamConnector,
                                    connector);
  }

  for (const std::string& tag : content_analysis_request_.tags())
    url = net::AppendQueryParameter(url, enterprise::kUrlParamTag, tag);

  return url;
}

bool BinaryUploadService::IsActive(Request* request) {
  return (active_requests_.find(request) != active_requests_.end());
}

class ValidateDataUploadRequest : public BinaryUploadService::Request {
 public:
  explicit ValidateDataUploadRequest(BinaryUploadService::Callback callback,
                                     GURL url)
      : BinaryUploadService::Request(std::move(callback), url) {}
  explicit ValidateDataUploadRequest(
      BinaryUploadService::ContentAnalysisCallback callback,
      GURL url)
      : BinaryUploadService::Request(std::move(callback), url) {}
  ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
  ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
      delete;
  ~ValidateDataUploadRequest() override = default;

 private:
  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;
};

inline void ValidateDataUploadRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                          BinaryUploadService::Request::Data());
}

void BinaryUploadService::IsAuthorized(const GURL& url,
                                       AuthorizationCallback callback) {
  // Start |timer_| on the first call to IsAuthorized. This is necessary in
  // order to invalidate the authorization every 24 hours.
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, base::TimeDelta::FromHours(24),
        base::BindRepeating(&BinaryUploadService::ResetAuthorizationData,
                            weakptr_factory_.GetWeakPtr(), url));
  }

  if (!can_upload_enterprise_data_.has_value()) {
    // Send a request to check if the browser can upload data.
    authorization_callbacks_.push_back(std::move(callback));
    if (!pending_validate_data_upload_request_) {
      auto dm_token = GetDMToken(profile_);
      if (!dm_token.is_valid()) {
        can_upload_enterprise_data_ = false;
        RunAuthorizationCallbacks();
        return;
      }

      pending_validate_data_upload_request_ = true;
      auto request =
          base::FeatureList::IsEnabled(
              enterprise_connectors::kEnterpriseConnectorsEnabled)
              ? std::make_unique<ValidateDataUploadRequest>(
                    base::BindOnce(
                        &BinaryUploadService::
                            ValidateDataUploadRequestConnectorCallback,
                        weakptr_factory_.GetWeakPtr()),
                    url)
              : std::make_unique<ValidateDataUploadRequest>(
                    base::BindOnce(
                        &BinaryUploadService::ValidateDataUploadRequestCallback,
                        weakptr_factory_.GetWeakPtr()),
                    url);
      request->set_device_token(dm_token.value());
      UploadForDeepScanning(std::move(request));
    }
    return;
  }
  std::move(callback).Run(can_upload_enterprise_data_.value());
}

void BinaryUploadService::ValidateDataUploadRequestConnectorCallback(
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  pending_validate_data_upload_request_ = false;
  can_upload_enterprise_data_ = result == BinaryUploadService::Result::SUCCESS;
}

void BinaryUploadService::ValidateDataUploadRequestCallback(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  pending_validate_data_upload_request_ = false;
  can_upload_enterprise_data_ = result == BinaryUploadService::Result::SUCCESS;
}

void BinaryUploadService::RunAuthorizationCallbacks() {
  DCHECK(can_upload_enterprise_data_.has_value());
  for (auto& callback : authorization_callbacks_) {
    std::move(callback).Run(can_upload_enterprise_data_.value());
  }
  authorization_callbacks_.clear();
}

void BinaryUploadService::ResetAuthorizationData(const GURL& url) {
  // Setting |can_upload_enterprise_data_| to base::nullopt will make the next
  // call to IsAuthorized send out a request to validate data uploads.
  can_upload_enterprise_data_ = base::nullopt;

  // Call IsAuthorized  to update |can_upload_enterprise_data_| right away.
  IsAuthorized(url, base::DoNothing());
}

void BinaryUploadService::Shutdown() {
  if (binary_fcm_service_)
    binary_fcm_service_->Shutdown();
}

void BinaryUploadService::SetAuthForTesting(bool authorized) {
  can_upload_enterprise_data_ = authorized;
}

// static
GURL BinaryUploadService::GetUploadUrl(bool is_advanced_protection) {
  return is_advanced_protection ? GURL(kSbAppUploadUrl)
                                : GURL(kSbEnterpriseUploadUrl);
}

}  // namespace safe_browsing
