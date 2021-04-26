// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom-test-utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace printing {

namespace {

constexpr int kDefaultDocumentCookie = 1234;

class PrintPreviewObserver : PrintPreviewUI::TestDelegate {
 public:
  explicit PrintPreviewObserver(bool wait_for_loaded) {
    if (wait_for_loaded)
      queue_.emplace();  // DOMMessageQueue doesn't allow assignment
    PrintPreviewUI::SetDelegateForTesting(this);
  }

  ~PrintPreviewObserver() override {
    PrintPreviewUI::SetDelegateForTesting(nullptr);
  }

  void WaitUntilPreviewIsReady() {
    if (rendered_page_count_ >= total_page_count_)
      return;

    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
    run_loop.Run();

    if (queue_.has_value()) {
      std::string message;
      EXPECT_TRUE(queue_->WaitForMessage(&message));
      EXPECT_EQ("\"success\"", message);
    }
  }

  content::WebContents* GetPrintPreviewDialog() { return preview_dialog_; }

 private:
  // PrintPreviewUI::TestDelegate:
  void DidGetPreviewPageCount(int page_count) override {
    total_page_count_ = page_count;
  }

  // PrintPreviewUI::TestDelegate:
  void DidRenderPreviewPage(content::WebContents* preview_dialog) override {
    ++rendered_page_count_;
    CHECK(rendered_page_count_ <= total_page_count_);
    if (rendered_page_count_ == total_page_count_ && run_loop_) {
      run_loop_->Quit();
      preview_dialog_ = preview_dialog;

      if (queue_.has_value()) {
        content::ExecuteScriptAsync(
            preview_dialog,
            "window.addEventListener('message', event => {"
            "  if (event.data.type === 'documentLoaded') {"
            "    domAutomationController.send(event.data.load_state);"
            "  }"
            "});");
      }
    }
  }

  base::Optional<content::DOMMessageQueue> queue_;
  int total_page_count_ = 1;
  int rendered_page_count_ = 0;
  content::WebContents* preview_dialog_ = nullptr;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewObserver);
};

class NupPrintingTestDelegate : public PrintingMessageFilter::TestDelegate {
 public:
  NupPrintingTestDelegate() {
    PrintingMessageFilter::SetDelegateForTesting(this);
  }
  ~NupPrintingTestDelegate() override {
    PrintingMessageFilter::SetDelegateForTesting(nullptr);
  }

  // PrintingMessageFilter::TestDelegate:
  PrintMsg_Print_Params GetPrintParams() override {
    PrintMsg_Print_Params params;
    params.page_size = gfx::Size(612, 792);
    params.content_size = gfx::Size(540, 720);
    params.printable_area = gfx::Rect(612, 792);
    params.dpi = gfx::Size(72, 72);
    params.document_cookie = kDefaultDocumentCookie;
    params.pages_per_sheet = 4;
    params.printed_doc_type = IsOopifEnabled() ? mojom::SkiaDocumentType::kMSKP
                                               : mojom::SkiaDocumentType::kPDF;
    return params;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NupPrintingTestDelegate);
};

class TestPrintRenderFrame
    : public mojom::PrintRenderFrameInterceptorForTesting {
 public:
  TestPrintRenderFrame(content::RenderFrameHost* frame_host,
                       content::WebContents* web_contents,
                       int document_cookie,
                       base::RepeatingClosure msg_callback)
      : frame_host_(frame_host),
        web_contents_(web_contents),
        document_cookie_(document_cookie),
        task_runner_(base::SequencedTaskRunnerHandle::Get()),
        msg_callback_(msg_callback) {}
  ~TestPrintRenderFrame() override = default;

  void OnDidPrintFrameContent(int document_cookie,
                              mojom::DidPrintContentParamsPtr param,
                              PrintFrameContentCallback callback) const {
    EXPECT_EQ(document_cookie, document_cookie_);
    ASSERT_TRUE(param->metafile_data_region.IsValid());
    EXPECT_GT(param->metafile_data_region.GetSize(), 0U);
    task_runner_->PostTask(FROM_HERE, msg_callback_);
    std::move(callback).Run(document_cookie, std::move(param));
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
        std::move(handle)));
  }

  // mojom::PrintRenderFrameInterceptorForTesting
  mojom::PrintRenderFrame* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }
  void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                         PrintFrameContentCallback callback) override {
    // Sends the printed result back.
    mojom::DidPrintContentParamsPtr printed_frame_params =
        mojom::DidPrintContentParams::New();
    // Creates a small amount of region to avoid passing empty data to mojo.
    constexpr size_t kSize = 10;
    base::MappedReadOnlyRegion region_mapping =
        base::ReadOnlySharedMemoryRegion::Create(kSize);
    printed_frame_params->metafile_data_region =
        std::move(region_mapping.region);
    OnDidPrintFrameContent(params->document_cookie,
                           std::move(printed_frame_params),
                           std::move(callback));

    auto* client = PrintCompositeClient::FromWebContents(web_contents_);
    if (!client)
      return;

    // Prints its children.
    content::RenderFrameHost* child = ChildFrameAt(frame_host_, 0);
    for (size_t i = 1; child; i++) {
      if (child->GetSiteInstance() != frame_host_->GetSiteInstance()) {
        client->PrintCrossProcessSubframe(gfx::Rect(), params->document_cookie,
                                          child);
      }
      child = ChildFrameAt(frame_host_, i);
    }
  }

 private:
  content::RenderFrameHost* frame_host_;
  content::WebContents* web_contents_;
  const int document_cookie_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::RepeatingClosure msg_callback_;
  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
};

class KillPrintRenderFrame
    : public mojom::PrintRenderFrameInterceptorForTesting {
 public:
  explicit KillPrintRenderFrame(content::RenderProcessHost* rph) : rph_(rph) {}
  ~KillPrintRenderFrame() override = default;

  void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host) {
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PrintRenderFrame::Name_,
            base::BindRepeating(&KillPrintRenderFrame::Bind,
                                base::Unretained(this)));
  }

  void KillRenderProcess(int document_cookie,
                         mojom::DidPrintContentParamsPtr param,
                         PrintFrameContentCallback callback) const {
    std::move(callback).Run(document_cookie, std::move(param));
    rph_->Shutdown(0);
  }

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame>(
        std::move(handle)));
  }

  // mojom::PrintRenderFrameInterceptorForTesting
  mojom::PrintRenderFrame* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }
  void PrintFrameContent(mojom::PrintFrameContentParamsPtr params,
                         PrintFrameContentCallback callback) override {
    // Sends the printed result back.
    const size_t kSize = 10;
    mojom::DidPrintContentParamsPtr printed_frame_params =
        mojom::DidPrintContentParams::New();
    base::MappedReadOnlyRegion region_mapping =
        base::ReadOnlySharedMemoryRegion::Create(kSize);
    printed_frame_params->metafile_data_region =
        std::move(region_mapping.region);
    KillRenderProcess(params->document_cookie, std::move(printed_frame_params),
                      std::move(callback));
  }

 private:
  content::RenderProcessHost* const rph_;
  mojo::AssociatedReceiver<mojom::PrintRenderFrame> receiver_{this};
};

}  // namespace

class PrintBrowserTest : public InProcessBrowserTest {
 public:
  PrintBrowserTest() = default;
  ~PrintBrowserTest() override = default;

  void SetUp() override {
    num_expected_messages_ = 1;  // By default, only wait on one message.
    num_received_messages_ = 0;
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void PrintAndWaitUntilPreviewIsReady(bool print_only_selection) {
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);

    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false, print_only_selection);

    print_preview_observer.WaitUntilPreviewIsReady();
  }

  void PrintAndWaitUntilPreviewIsReadyAndLoaded(bool print_only_selection) {
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);

    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false, print_only_selection);

    print_preview_observer.WaitUntilPreviewIsReady();
  }

  // The following are helper functions for having a wait loop in the test and
  // exit when all expected messages are received.
  void SetNumExpectedMessages(unsigned int num) {
    num_expected_messages_ = num;
  }

  void WaitUntilCallbackReceived() {
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CheckForQuit() {
    if (++num_received_messages_ != num_expected_messages_)
      return;
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }

  void CreateTestPrintRenderFrame(content::RenderFrameHost* frame_host,
                                  content::WebContents* web_contents) {
    frame_content_.emplace(
        frame_host, std::make_unique<TestPrintRenderFrame>(
                        frame_host, web_contents, kDefaultDocumentCookie,
                        base::BindRepeating(&PrintBrowserTest::CheckForQuit,
                                            base::Unretained(this))));
    OverrideBinderForTesting(frame_host);
  }

  static mojom::PrintFrameContentParamsPtr GetDefaultPrintFrameParams() {
    return mojom::PrintFrameContentParams::New(gfx::Rect(800, 600),
                                               kDefaultDocumentCookie);
  }

  const mojo::AssociatedRemote<mojom::PrintRenderFrame>& GetPrintRenderFrame(
      content::RenderFrameHost* rfh) {
    if (!remote_)
      rfh->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
    return remote_;
  }

 private:
  TestPrintRenderFrame* GetFrameContent(content::RenderFrameHost* host) const {
    auto iter = frame_content_.find(host);
    return iter != frame_content_.end() ? iter->second.get() : nullptr;
  }

  void OverrideBinderForTesting(content::RenderFrameHost* render_frame_host) {
    render_frame_host->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::PrintRenderFrame::Name_,
            base::BindRepeating(
                &TestPrintRenderFrame::Bind,
                base::Unretained(GetFrameContent(render_frame_host))));
  }

  unsigned int num_expected_messages_;
  unsigned int num_received_messages_;
  base::OnceClosure quit_callback_;
  mojo::AssociatedRemote<mojom::PrintRenderFrame> remote_;
  std::map<content::RenderFrameHost*, std::unique_ptr<TestPrintRenderFrame>>
      frame_content_;
};

class SitePerProcessPrintBrowserTest : public PrintBrowserTest {
 public:
  SitePerProcessPrintBrowserTest() = default;
  ~SitePerProcessPrintBrowserTest() override = default;

  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }
};

class IsolateOriginsPrintBrowserTest : public PrintBrowserTest {
 public:
  static constexpr char kIsolatedSite[] = "b.com";

  IsolateOriginsPrintBrowserTest() = default;
  ~IsolateOriginsPrintBrowserTest() override = default;

  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    std::string origin_list =
        embedded_test_server()->GetURL(kIsolatedSite, "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

class BackForwardCachePrintBrowserTest : public PrintBrowserTest {
 public:
  BackForwardCachePrintBrowserTest() = default;
  ~BackForwardCachePrintBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kBackForwardCache,
        {
            // Set a very long TTL before expiration (longer than the test
            // timeout) so tests that are expecting deletion don't pass when
            // they shouldn't.
            {"TimeToLiveInBackForwardCacheInSeconds", "3600"},
        });

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

  void ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature feature,
      base::Location location) {
    base::HistogramBase::Sample sample = base::HistogramBase::Sample(feature);
    AddSampleToBuckets(&expected_blocklisted_features_, sample);

    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "BackForwardCache.HistoryNavigationOutcome."
            "BlocklistedFeature"),
        testing::UnorderedElementsAreArray(expected_blocklisted_features_))
        << location.ToString();

    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            "BackForwardCache.AllSites.HistoryNavigationOutcome."
            "BlocklistedFeature"),
        testing::UnorderedElementsAreArray(expected_blocklisted_features_))
        << location.ToString();
  }

  base::HistogramTester histogram_tester_;

 private:
  void AddSampleToBuckets(std::vector<base::Bucket>* buckets,
                          base::HistogramBase::Sample sample) {
    auto it = std::find_if(
        buckets->begin(), buckets->end(),
        [sample](const base::Bucket& bucket) { return bucket.min == sample; });
    if (it == buckets->end()) {
      buckets->push_back(base::Bucket(sample, 1));
    } else {
      it->count++;
    }
  }

  std::vector<base::Bucket> expected_blocklisted_features_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCachePrintBrowserTest);
};

constexpr char IsolateOriginsPrintBrowserTest::kIsolatedSite[];

class PrintExtensionBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  PrintExtensionBrowserTest() = default;
  ~PrintExtensionBrowserTest() override = default;

  void PrintAndWaitUntilPreviewIsReady(bool print_only_selection) {
    PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/false);

    StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
               /*print_renderer=*/mojo::NullAssociatedRemote(),
               /*print_preview_disabled=*/false, print_only_selection);

    print_preview_observer.WaitUntilPreviewIsReady();
  }

  void LoadExtensionAndNavigateToOptionPage() {
    const extensions::Extension* extension = nullptr;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FilePath test_data_dir;
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
      extension = LoadExtension(
          test_data_dir.AppendASCII("printing").AppendASCII("test_extension"));
      ASSERT_TRUE(extension);
    }

    GURL url(chrome::kChromeUIExtensionsURL);
    std::string query =
        base::StringPrintf("options=%s", extension->id().c_str());
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    url = url.ReplaceComponents(replacements);
    ui_test_utils::NavigateToURL(browser(), url);
  }
};

class SitePerProcessPrintExtensionBrowserTest
    : public PrintExtensionBrowserTest {
 public:
  // content::BrowserTestBase
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }
};

// Printing only a selection containing iframes is partially supported.
// Iframes aren't currently displayed. This test passes whenever the print
// preview is rendered (i.e. no timeout in the test).
// This test shouldn't crash. See https://crbug.com/732780.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, SelectionContainsIframe) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/selection_iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/true);
}

// Printing frame content for the main frame of a generic webpage.
// This test passes when the printed result is sent back and checked in
// TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintFrameContent) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = original_contents->GetMainFrame();
  CreateTestPrintRenderFrame(rfh, original_contents);
  GetPrintRenderFrame(rfh)->PrintFrameContent(GetDefaultPrintFrameParams(),
                                              base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  WaitUntilCallbackReceived();
}

// Printing frame content for a cross-site iframe.
// This test passes when the iframe responds to the print message.
// The response is checked in TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintSubframeContent) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(2u, original_contents->GetAllFrames().size());
  content::RenderFrameHost* test_frame = original_contents->GetAllFrames()[1];
  ASSERT_TRUE(test_frame);

  CreateTestPrintRenderFrame(test_frame, original_contents);
  GetPrintRenderFrame(test_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  WaitUntilCallbackReceived();
}

// Printing frame content with a cross-site iframe which also has a cross-site
// iframe. The site reference chain is a.com --> b.com --> c.com.
// This test passes when both cross-site frames are printed and their
// responses which are checked in
// TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintSubframeChain) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/content_with_iframe_chain.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(3u, original_contents->GetAllFrames().size());
  // Create composite client so subframe print message can be forwarded.
  PrintCompositeClient::CreateForWebContents(original_contents);

  content::RenderFrameHost* main_frame = original_contents->GetMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_NE(child_frame, main_frame);
  bool oopif_enabled = child_frame->GetProcess() != main_frame->GetProcess();

  content::RenderFrameHost* grandchild_frame =
      content::ChildFrameAt(child_frame, 0);
  ASSERT_TRUE(grandchild_frame);
  ASSERT_NE(grandchild_frame, child_frame);
  if (oopif_enabled) {
    ASSERT_NE(grandchild_frame->GetProcess(), child_frame->GetProcess());
    ASSERT_NE(grandchild_frame->GetProcess(), main_frame->GetProcess());
  }

  CreateTestPrintRenderFrame(main_frame, original_contents);
  if (oopif_enabled) {
    CreateTestPrintRenderFrame(child_frame, original_contents);
    CreateTestPrintRenderFrame(grandchild_frame, original_contents);
  }

  GetPrintRenderFrame(main_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  SetNumExpectedMessages(oopif_enabled ? 3 : 1);
  WaitUntilCallbackReceived();
}

// Printing frame content with a cross-site iframe who also has a cross site
// iframe, but this iframe resides in the same site as the main frame.
// The site reference loop is a.com --> b.com --> a.com.
// This test passes when both cross-site frames are printed and send back
// responses which are checked in
// TestPrintRenderFrame::OnDidPrintFrameContent().
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintSubframeABA) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/printing/content_with_iframe_loop.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(3u, original_contents->GetAllFrames().size());
  // Create composite client so subframe print message can be forwarded.
  PrintCompositeClient::CreateForWebContents(original_contents);

  content::RenderFrameHost* main_frame = original_contents->GetMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  ASSERT_NE(child_frame, main_frame);
  bool oopif_enabled = main_frame->GetProcess() != child_frame->GetProcess();

  content::RenderFrameHost* grandchild_frame =
      content::ChildFrameAt(child_frame, 0);
  ASSERT_TRUE(grandchild_frame);
  ASSERT_NE(grandchild_frame, child_frame);
  // |grandchild_frame| is in the same site as |frame|, so whether OOPIF is
  // enabled, they will be in the same process.
  ASSERT_EQ(grandchild_frame->GetProcess(), main_frame->GetProcess());

  CreateTestPrintRenderFrame(main_frame, original_contents);
  if (oopif_enabled) {
    CreateTestPrintRenderFrame(child_frame, original_contents);
    CreateTestPrintRenderFrame(grandchild_frame, original_contents);
  }

  GetPrintRenderFrame(main_frame)
      ->PrintFrameContent(GetDefaultPrintFrameParams(), base::DoNothing());

  // The printed result will be received and checked in
  // TestPrintRenderFrame.
  SetNumExpectedMessages(oopif_enabled ? 3 : 1);
  WaitUntilCallbackReceived();
}

// Printing preview a simple webpage when site per process is enabled.
// Test that the basic oopif printing should succeed. The test should not crash
// or timed out. There could be other reasons that cause the test fail, but the
// most obvious ones would be font access outage or web sandbox support being
// absent because we explicitly check these when pdf compositor service starts.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, BasicPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// Printing a web page with a dead subframe for site per process should succeed.
// This test passes whenever the print preview is rendered. This should not be
// a timed out test which indicates the print preview hung.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest,
                       SubframeUnavailableBeforePrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(2u, original_contents->GetAllFrames().size());
  content::RenderFrameHost* test_frame = original_contents->GetAllFrames()[1];
  ASSERT_TRUE(test_frame);
  ASSERT_TRUE(test_frame->IsRenderFrameLive());
  // Wait for the renderer to be down.
  content::RenderProcessHostWatcher render_process_watcher(
      test_frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  // Shutdown the subframe.
  ASSERT_TRUE(test_frame->GetProcess()->Shutdown(0));
  render_process_watcher.Wait();
  ASSERT_FALSE(test_frame->IsRenderFrameLive());

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// If a subframe dies during printing, the page printing should still succeed.
// This test passes whenever the print preview is rendered. This should not be
// a timed out test which indicates the print preview hung.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest,
                       SubframeUnavailableDuringPrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(
      embedded_test_server()->GetURL("/printing/content_with_iframe.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(2u, original_contents->GetAllFrames().size());
  content::RenderFrameHost* subframe = original_contents->GetAllFrames()[1];
  ASSERT_TRUE(subframe);
  auto* subframe_rph = subframe->GetProcess();

  KillPrintRenderFrame frame_content(subframe_rph);
  frame_content.OverrideBinderForTesting(subframe);
  content::ScopedAllowRendererCrashes allow_renderer_crashes(subframe_rph);

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// Printing preview a web page with an iframe from an isolated origin.
// This test passes whenever the print preview is rendered. This should not be
// a timed out test which indicates the print preview hung or crash.
IN_PROC_BROWSER_TEST_F(IsolateOriginsPrintBrowserTest,
                       DISABLED_PrintIsolatedSubframe) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL(
      "/printing/content_with_same_site_iframe.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL(kIsolatedSite, "/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(NavigateIframeToURL(original_contents, "iframe", isolated_url));

  ASSERT_EQ(2u, original_contents->GetAllFrames().size());
  auto* main_frame = original_contents->GetMainFrame();
  auto* subframe = original_contents->GetAllFrames()[1];
  ASSERT_NE(main_frame->GetProcess(), subframe->GetProcess());

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// Printing preview a webpage.
// Test that we use oopif printing by default.
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, RegularPrinting) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(IsOopifEnabled());
}

// Printing preview a webpage with isolate-origins enabled.
// Test that we will use oopif printing for this case.
IN_PROC_BROWSER_TEST_F(IsolateOriginsPrintBrowserTest, OopifPrinting) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(IsOopifEnabled());
}

IN_PROC_BROWSER_TEST_F(BackForwardCachePrintBrowserTest, DisableCaching) {
  ASSERT_TRUE(embedded_test_server()->Started());

  // 1) Navigate to A and trigger printing.
  GURL url(embedded_test_server()->GetURL("a.com", "/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::RenderFrameHost* rfh_a = current_frame_host();
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);

  // 2) Navigate to B.
  // The first page is not cached because printing preview was open.
  GURL url_2(embedded_test_server()->GetURL("b.com", "/printing/test2.html"));
  ui_test_utils::NavigateToURL(browser(), url_2);
  delete_observer_rfh_a.WaitUntilDeleted();

  // 3) Navigate back and checks the blocklisted feature is recorded in UMA.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  ExpectBlocklistedFeature(
      blink::scheduler::WebSchedulerTrackedFeature::kPrinting, FROM_HERE);
}

// Printing an extension option page.
// The test should not crash or timeout.
IN_PROC_BROWSER_TEST_F(PrintExtensionBrowserTest, PrintOptionPage) {
  LoadExtensionAndNavigateToOptionPage();
  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// Printing an extension option page with site per process is enabled.
// The test should not crash or timeout.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintExtensionBrowserTest,
                       PrintOptionPage) {
  LoadExtensionAndNavigateToOptionPage();
  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// Printing frame content for the main frame of a generic webpage with N-up
// priting. This is a regression test for https://crbug.com/937247
IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PrintNup) {
  NupPrintingTestDelegate test_delegate;
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

// Site per process version of PrintBrowserTest.PrintNup.
IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, PrintNup) {
  NupPrintingTestDelegate test_delegate;
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/test1.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReady(/*print_only_selection=*/false);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, MultipagePrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReadyAndLoaded(/*print_only_selection=*/false);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessPrintBrowserTest, MultipagePrint) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintAndWaitUntilPreviewIsReadyAndLoaded(/*print_only_selection=*/false);
}

IN_PROC_BROWSER_TEST_F(PrintBrowserTest, PDFPluginNotKeyboardFocusable) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url(embedded_test_server()->GetURL("/printing/multipage.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  PrintPreviewObserver print_preview_observer(/*wait_for_loaded=*/true);
  StartPrint(browser()->tab_strip_model()->GetActiveWebContents(),
             /*print_renderer=*/mojo::NullAssociatedRemote(),
             /*print_preview_disabled=*/false, /*print_only_selection=*/false);
  print_preview_observer.WaitUntilPreviewIsReady();

  content::WebContents* preview_dialog =
      print_preview_observer.GetPrintPreviewDialog();
  ASSERT_TRUE(preview_dialog);

  // The script will ensure we return the id of <zoom-out-button> when
  // focused. Focus the element after PDF plugin in tab order.
  const char kScript[] = R"(
    const button = document.getElementsByTagName('print-preview-app')[0]
                       .$['previewArea']
                       .$$('iframe')
                       .contentDocument.querySelector('pdf-viewer-pp')
                       .shadowRoot.querySelector('#zoom-toolbar')
                       .$['zoom-out-button'];
    button.addEventListener('focus', (e) => {
      window.domAutomationController.send(e.target.id);
    });

    const select_tag = document.getElementsByTagName('print-preview-app')[0]
                           .$['sidebar']
                           .$['destinationSettings']
                           .$['destinationSelect']
                           .$$('select');
    select_tag.addEventListener('focus', () => {
      window.domAutomationController.send(true);
    });
    select_tag.focus();)";
  bool success = false;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(preview_dialog, kScript, &success));
  ASSERT_TRUE(success);

  // Simulate a <shift-tab> press and wait for a focus message.
  content::DOMMessageQueue msg_queue;
  SimulateKeyPress(preview_dialog, ui::DomKey::TAB, ui::DomCode::TAB,
                   ui::VKEY_TAB, false, true, false, false);
  std::string reply;
  ASSERT_TRUE(msg_queue.WaitForMessage(&reply));
  // Pressing <shift-tab> should focus the last toolbar element
  // (zoom-out-button) instead of PDF plugin.
  EXPECT_EQ("\"zoom-out-button\"", reply);
}

}  // namespace printing
