// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_utils.h"

namespace safe_browsing {

namespace {

// Extract the metadata proto from the raw request string. Returns true on
// success.
template <typename T>
bool GetUploadMetadata(const std::string& upload_request, T* out_proto) {
  // The request is of the following format, see multipart_uploader.h for
  // details:
  // ---MultipartBoundary---
  // <Headers for metadata>
  //
  // <Base64-encoded metadata>
  // ---MultipartBoundary---
  // <Headers for uploaded data>
  //
  // <Uploaded data>
  // ---MultipartBoundary---
  size_t boundary_end = upload_request.find("\r\n");
  std::string multipart_boundary = upload_request.substr(0, boundary_end);

  size_t headers_end = upload_request.find("\r\n\r\n");
  size_t metadata_end =
      upload_request.find("\r\n" + multipart_boundary, headers_end);
  std::string encoded_metadata =
      upload_request.substr(headers_end + 4, metadata_end - headers_end - 4);

  std::string serialized_metadata;
  base::Base64Decode(encoded_metadata, &serialized_metadata);
  return out_proto->ParseFromString(serialized_metadata);
}

}  // namespace

class FakeBinaryFCMService : public BinaryFCMService {
 public:
  FakeBinaryFCMService() {}

  void GetInstanceID(GetInstanceIDCallback callback) override {
    std::move(callback).Run("test_instance_id");
  }

  void UnregisterInstanceID(const std::string& token,
                            UnregisterInstanceIDCallback callback) override {
    // Always successfully unregister.
    std::move(callback).Run(true);
  }
};

// Integration tests for download deep scanning behavior, only mocking network
// traffic and FCM dependencies.
class DownloadDeepScanningBrowserTest
    : public DeepScanningBrowserTestBase,
      public content::DownloadManager::Observer,
      public download::DownloadItem::Observer,
      public testing::WithParamInterface<bool> {
 public:
  DownloadDeepScanningBrowserTest()
      : DeepScanningBrowserTestBase(use_legacy_policies()) {}

  bool use_legacy_policies() const { return GetParam(); }

  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    item->AddObserver(this);
    download_items_.insert(item);
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    download_items_.erase(item);
  }

 protected:
  void SetUp() override {
    test_sb_factory_ = std::make_unique<TestSafeBrowsingServiceFactory>();
    test_sb_factory_->UseV4LocalDatabaseManager();
    SafeBrowsingService::RegisterFactory(test_sb_factory_.get());

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();

    SafeBrowsingService::RegisterFactory(nullptr);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
    AddUrlToCheckComplianceOfDownloads(
        embedded_test_server()->base_url().spec());

    SetBinaryUploadServiceTestFactory();
    SetUrlLoaderInterceptor();
    ObserveDownloadManager();
    AuthorizeForDeepScanning();

    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));
    SetDlpPolicy(CheckContentComplianceValues::CHECK_DOWNLOADS);
    SetMalwarePolicy(SendFilesForMalwareCheckValues::SEND_DOWNLOADS);
  }

  void WaitForDownloadToFinish() {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    observer.WaitForFinished();
  }

  void WaitForDeepScanRequest(bool is_advanced_protection) {
    if (is_advanced_protection)
      waiting_for_app_ = true;
    else
      waiting_for_enterprise_ = true;

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    waiting_for_upload_closure_ = run_loop.QuitClosure();
    run_loop.Run();

    waiting_for_app_ = false;
    waiting_for_enterprise_ = false;
  }

  void ExpectMetadataResponse(const ClientDownloadResponse& response) {
    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
                      response.SerializeAsString());
  }

  void ExpectDeepScanSynchronousResponse(
      bool is_advanced_protection,
      const DeepScanningClientResponse& response) {
    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(
            BinaryUploadService::GetUploadUrl(is_advanced_protection).spec(),
            response.SerializeAsString());
  }

  void ExpectContentAnalysisSynchronousResponse(
      bool is_advanced_protection,
      const enterprise_connectors::ContentAnalysisResponse& response,
      const std::vector<std::string>& tags) {
    connector_url_ =
        "https://safebrowsing.google.com/safebrowsing/uploads/"
        "scan?device_token=dm_token&connector=OnFileDownloaded";
    for (const std::string& tag : tags)
      connector_url_ += ("&tag=" + tag);

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(connector_url_, response.SerializeAsString());
  }

  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

  FakeBinaryFCMService* binary_fcm_service() { return binary_fcm_service_; }

  TestSafeBrowsingServiceFactory* test_sb_factory() {
    return test_sb_factory_.get();
  }

  const enterprise_connectors::ContentAnalysisRequest&
  last_app_content_analysis_request() {
    return last_app_content_analysis_request_;
  }

  const DeepScanningClientRequest& last_app_request() {
    return last_app_request_;
  }

  const enterprise_connectors::ContentAnalysisRequest&
  last_enterprise_content_analysis_request() {
    return last_enterprise_content_analysis_request_;
  }

  const DeepScanningClientRequest& last_enterprise_request() {
    return last_enterprise_request_;
  }

  const base::flat_set<download::DownloadItem*>& download_items() {
    return download_items_;
  }

  void SetBinaryUploadServiceTestFactory() {
    BinaryUploadServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DownloadDeepScanningBrowserTest::CreateBinaryUploadService,
            base::Unretained(this)));
  }

  void ObserveDownloadManager() {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    download_manager->AddObserver(this);
  }

  void SetUrlLoaderInterceptor() {
    test_sb_factory()->test_safe_browsing_service()->SetUseTestUrlLoaderFactory(
        true);
    test_sb_factory()
        ->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->SetInterceptor(base::BindRepeating(
            &DownloadDeepScanningBrowserTest::InterceptRequest,
            base::Unretained(this)));
  }

  template <typename T>
  void SendFcmMessage(const T& response) {
    std::string encoded_proto;
    base::Base64Encode(response.SerializeAsString(), &encoded_proto);
    gcm::IncomingMessage gcm_message;
    gcm_message.data["proto"] = encoded_proto;
    binary_fcm_service()->OnMessage("app_id", gcm_message);
  }

  void AuthorizeForDeepScanning() {
    BinaryUploadServiceFactory::GetForProfile(browser()->profile())
        ->SetAuthForTesting(/*authorized=*/true);
  }

 private:
  std::unique_ptr<KeyedService> CreateBinaryUploadService(
      content::BrowserContext* browser_context) {
    std::unique_ptr<FakeBinaryFCMService> binary_fcm_service =
        std::make_unique<FakeBinaryFCMService>();
    binary_fcm_service_ = binary_fcm_service.get();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    return std::make_unique<BinaryUploadService>(
        g_browser_process->safe_browsing_service()->GetURLLoaderFactory(),
        profile, std::move(binary_fcm_service));
  }

  void InterceptRequest(const network::ResourceRequest& request) {
    if (request.url ==
        BinaryUploadService::GetUploadUrl(/*is_advanced_protection=*/true)) {
      if (use_legacy_policies()) {
        ASSERT_TRUE(GetUploadMetadata(network::GetUploadData(request),
                                      &last_app_request_));
      } else {
        ASSERT_TRUE(GetUploadMetadata(network::GetUploadData(request),
                                      &last_app_content_analysis_request_));
      }
      if (waiting_for_app_)
        std::move(waiting_for_upload_closure_).Run();
    }

    if (request.url ==
        BinaryUploadService::GetUploadUrl(/*is_advanced_protection=*/false)) {
      if (use_legacy_policies()) {
        ASSERT_TRUE(GetUploadMetadata(network::GetUploadData(request),
                                      &last_enterprise_request_));
      } else {
        ASSERT_TRUE(
            GetUploadMetadata(network::GetUploadData(request),
                              &last_enterprise_content_analysis_request_));
      }
      if (waiting_for_enterprise_)
        std::move(waiting_for_upload_closure_).Run();
    }

    if (request.url == connector_url_) {
      ASSERT_TRUE(!use_legacy_policies());
      ASSERT_TRUE(
          GetUploadMetadata(network::GetUploadData(request),
                            &last_enterprise_content_analysis_request_));
      if (waiting_for_enterprise_)
        std::move(waiting_for_upload_closure_).Run();
    }
  }

  std::unique_ptr<TestSafeBrowsingServiceFactory> test_sb_factory_;
  FakeBinaryFCMService* binary_fcm_service_;

  bool waiting_for_app_;
  enterprise_connectors::ContentAnalysisRequest
      last_app_content_analysis_request_;
  DeepScanningClientRequest last_app_request_;

  bool waiting_for_enterprise_;
  enterprise_connectors::ContentAnalysisRequest
      last_enterprise_content_analysis_request_;
  DeepScanningClientRequest last_enterprise_request_;

  std::string connector_url_;

  base::OnceClosure waiting_for_upload_closure_;

  base::flat_set<download::DownloadItem*> download_items_;
};

INSTANTIATE_TEST_SUITE_P(, DownloadDeepScanningBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       SafeDownloadHasCorrectDangerType) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, but doesn't find anything.
  if (use_legacy_policies()) {
    DeepScanningClientResponse sync_response;
    sync_response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
    ExpectDeepScanSynchronousResponse(/*is_advanced_protection=*/false,
                                      sync_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    auto* result = sync_response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                             sync_response, {"dlp", "malware"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and doesn't find anything.
  if (use_legacy_policies()) {
    DeepScanningClientResponse async_response;
    async_response.set_token(last_enterprise_request().request_token());
    async_response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::CLEAN);
    SendFcmMessage(async_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse async_response;
    async_response.set_request_token(
        last_enterprise_content_analysis_request().request_token());
    auto* result = async_response.add_results();
    result->set_tag("malware");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    SendFcmMessage(async_response);
  }

  WaitForDownloadToFinish();

  // The file should be deep scanned, and safe.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest, FailedScanFailsOpen) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, but doesn't find anything.
  if (use_legacy_policies()) {
    DeepScanningClientResponse sync_response;
    sync_response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
    ExpectDeepScanSynchronousResponse(/*is_advanced_protection=*/false,
                                      sync_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    auto* result = sync_response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                             sync_response, {"dlp", "malware"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and fails
  if (use_legacy_policies()) {
    DeepScanningClientResponse async_response;
    async_response.set_token(last_enterprise_request().request_token());
    async_response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::SCAN_FAILURE);
    SendFcmMessage(async_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse async_response;
    async_response.set_request_token(
        last_enterprise_content_analysis_request().request_token());
    auto* result = async_response.add_results();
    result->set_tag("malware");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    SendFcmMessage(async_response);
  }

  WaitForDownloadToFinish();

  // The file should be safe, but not deep scanned.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       PartialFailureShowsMalwareWarning) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, and fails.
  if (use_legacy_policies()) {
    DeepScanningClientResponse sync_response;
    sync_response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::FAILURE);
    ExpectDeepScanSynchronousResponse(/*is_advanced_protection=*/false,
                                      sync_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    auto* result = sync_response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                             sync_response, {"dlp", "malware"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and finds malware.
  if (use_legacy_policies()) {
    DeepScanningClientResponse async_response;
    async_response.set_token(last_enterprise_request().request_token());
    async_response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::MALWARE);
    SendFcmMessage(async_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse async_response;
    async_response.set_request_token(
        last_enterprise_content_analysis_request().request_token());
    auto* result = async_response.add_results();
    result->set_tag("malware");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    auto* malware_rule = result->add_triggered_rules();
    malware_rule->set_action(enterprise_connectors::ContentAnalysisResponse::
                                 Result::TriggeredRule::BLOCK);
    malware_rule->set_rule_name("MALWARE");
    SendFcmMessage(async_response);
  }

  WaitForDownloadToFinish();

  // The file should be dangerous.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);
}

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       PartialFailureShowsDlpWarning) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, and finds a violation.
  if (use_legacy_policies()) {
    DeepScanningClientResponse sync_response;
    sync_response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
    sync_response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    ExpectDeepScanSynchronousResponse(/*is_advanced_protection=*/false,
                                      sync_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    auto* result = sync_response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    auto* dlp_rule = result->add_triggered_rules();
    dlp_rule->set_action(enterprise_connectors::ContentAnalysisResponse::
                             Result::TriggeredRule::BLOCK);
    ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                             sync_response, {"dlp", "malware"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and fails.
  if (use_legacy_policies()) {
    DeepScanningClientResponse async_response;
    async_response.set_token(last_enterprise_request().request_token());
    async_response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::SCAN_FAILURE);
    SendFcmMessage(async_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse async_response;
    async_response.set_request_token(
        last_enterprise_content_analysis_request().request_token());
    auto* result = async_response.add_results();
    result->set_tag("malware");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    SendFcmMessage(async_response);
  }

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       DangerousHostNotMalwareScanned) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan still runs, but finds nothing
  if (use_legacy_policies()) {
    DeepScanningClientResponse sync_response;
    sync_response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
    ExpectDeepScanSynchronousResponse(/*is_advanced_protection=*/false,
                                      sync_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    auto* result = sync_response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                             sync_response, {"dlp", "malware"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/signed.exe");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDownloadToFinish();

  // The file should be blocked.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);
}

class WhitelistedUrlDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTest {
 public:
  WhitelistedUrlDeepScanningBrowserTest() = default;
  ~WhitelistedUrlDeepScanningBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTest::SetUpOnMainThread();

    base::ListValue domain_list;
    domain_list.AppendString(embedded_test_server()->base_url().host_piece());
    browser()->profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains,
                                          domain_list);
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         WhitelistedUrlDeepScanningBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(WhitelistedUrlDeepScanningBrowserTest,
                       WhitelistedUrlStillDoesDlp) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, and finds a violation.
  if (use_legacy_policies()) {
    DeepScanningClientResponse sync_response;
    sync_response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
    sync_response.mutable_dlp_scan_verdict()->add_triggered_rules()->set_action(
        DlpDeepScanningVerdict::TriggeredRule::BLOCK);
    ExpectDeepScanSynchronousResponse(/*is_advanced_protection=*/false,
                                      sync_response);
  } else {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    auto* result = sync_response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    auto* dlp_rule = result->add_triggered_rules();
    dlp_rule->set_action(enterprise_connectors::ContentAnalysisResponse::
                             Result::TriggeredRule::BLOCK);
    ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                             sync_response, {"dlp"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

}  // namespace safe_browsing
