// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROXYING_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

class Profile;

// This class is an intermediary URLLoaderFactory between the renderer and
// network process, AKA proxy which should not be confused with a proxy server.
//
// This class sends all requests to an isolated network context which will strip
// any private information before being sent on the wire. Those requests are
// also monitored for when resource loads complete successfully and reports
// those to the |IsolatedPrerenderSubresourceManager| which owns |this|.
class IsolatedPrerenderProxyingURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  using DisconnectCallback =
      base::OnceCallback<void(IsolatedPrerenderProxyingURLLoaderFactory*)>;

  using ResourceLoadSuccessfulCallback =
      base::RepeatingCallback<void(const GURL& url)>;

  IsolatedPrerenderProxyingURLLoaderFactory(
      int frame_tree_node_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          network_process_factory,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory,
      DisconnectCallback on_disconnect,
      ResourceLoadSuccessfulCallback on_resource_load_successful);
  ~IsolatedPrerenderProxyingURLLoaderFactory() override;

  // Informs |this| that new subresource loads are being done after the user
  // clicked on a link that was previously prerendered. From this point on, all
  // requests for resources in |cached_subresources| will be done from
  // |isolated_factory_|'s cache and any other request will be done by
  // |network_process_factory_|.
  void NotifyPageNavigatedToAfterSRP(const std::set<GURL>& cached_subresources);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

 private:
  class InProgressRequest : public network::mojom::URLLoader,
                            public network::mojom::URLLoaderClient {
   public:
    InProgressRequest(
        Profile* profile,
        IsolatedPrerenderProxyingURLLoaderFactory* parent_factory,
        network::mojom::URLLoaderFactory* target_factory,
        ResourceLoadSuccessfulCallback on_resource_load_successful,
        mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);
    ~InProgressRequest() override;

    // Sets a callback that will be run during |OnComplete| to record metrics.
    using OnCompleteRecordMetricsCallback = base::OnceCallback<void(
        const network::URLLoaderCompletionStatus& status,
        base::Optional<int> http_response_code)>;
    void SetOnCompleteRecordMetricsCallback(
        OnCompleteRecordMetricsCallback callback);

    // network::mojom::URLLoader:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const base::Optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

    // network::mojom::URLLoaderClient:
    void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override;
    void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                           network::mojom::URLResponseHeadPtr head) override;
    void OnUploadProgress(int64_t current_position,
                          int64_t total_size,
                          OnUploadProgressCallback callback) override;
    void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
    void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
    void OnStartLoadingResponseBody(
        mojo::ScopedDataPipeConsumerHandle body) override;
    void OnComplete(const network::URLLoaderCompletionStatus& status) override;

   private:
    void OnBindingsClosed();

    // Runs |on_resource_load_successful_| for each url in |redirect_chain_| if
    // the resource was successfully loaded.
    void MaybeReportResourceLoadSuccess(
        const network::URLLoaderCompletionStatus& status);

    Profile* profile_;

    // Back pointer to the factory which owns this class.
    IsolatedPrerenderProxyingURLLoaderFactory* const parent_factory_;

    // Callback for recording metrics during |OnComplete|. Not always set.
    OnCompleteRecordMetricsCallback on_complete_metrics_callback_;

    // This should be run on destruction of |this|.
    base::OnceClosure destruction_callback_;

    // Records the HTTP response code in |OnReceiveResponse|.
    base::Optional<int> http_response_code_;

    // All urls loaded by |this| in order of redirects. The first element is the
    // requested url and the last element is the final loaded url. Always has
    // length of at least 1.
    std::vector<GURL> redirect_chain_;

    // Used to report successfully loaded urls in the redirect chain.
    ResourceLoadSuccessfulCallback on_resource_load_successful_;

    // There are the mojo pipe endpoints between this proxy and the renderer.
    // Messages received by |client_receiver_| are forwarded to
    // |target_client_|.
    mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
    mojo::Remote<network::mojom::URLLoaderClient> target_client_;

    // These are the mojo pipe endpoints between this proxy and the network
    // process. Messages received by |loader_receiver_| are forwarded to
    // |target_loader_|.
    mojo::Receiver<network::mojom::URLLoader> loader_receiver_;
    mojo::Remote<network::mojom::URLLoader> target_loader_;

    DISALLOW_COPY_AND_ASSIGN(InProgressRequest);
  };

  // Used as a callback for determining the eligibility of a resource to be
  // cached during prerender.
  void OnEligibilityResult(
      Profile* profile,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const GURL& url,
      bool eligible,
      base::Optional<IsolatedPrerenderTabHelper::PrefetchStatus> not_used);

  // Returns true when this factory was created during a NoStatePrefetch.
  // Internally, this means |NotifyPageNavigatedToAfterSRP| has not been called.
  bool ShouldHandleRequestForPrerender() const;

  void OnNetworkProcessFactoryError();
  void OnIsolatedFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InProgressRequest* request);
  void MaybeDestroySelf();

  // For getting the web contents.
  const int frame_tree_node_id_;

  // When |previously_cached_subresources_| is set,
  // |NotifyPageNavigatedToAfterSRP| has been called and the behavior there will
  // take place using this set as the resources that can be loaded from cache.
  base::Optional<std::set<GURL>> previously_cached_subresources_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;

  // Passed to each InProgressRequest so they can report successfully loaded
  // urls in their redirect chain.
  ResourceLoadSuccessfulCallback on_resource_load_successful_;

  // All active network requests handled by this factory.
  std::set<std::unique_ptr<InProgressRequest>, base::UniquePtrComparator>
      requests_;

  // The network process URLLoaderFactory.
  mojo::Remote<network::mojom::URLLoaderFactory> network_process_factory_;

  // The isolated URLLoaderFactory.
  mojo::Remote<network::mojom::URLLoaderFactory> isolated_factory_;

  // Deletes |this| when run.
  DisconnectCallback on_disconnect_;

  base::WeakPtrFactory<IsolatedPrerenderProxyingURLLoaderFactory> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderProxyingURLLoaderFactory);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROXYING_URL_LOADER_FACTORY_H_
