// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage_control_impl.h"

#include "content/browser/service_worker/service_worker_resource_ops.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

void DidGetRegistrationsForOrigin(
    ServiceWorkerStorageControlImpl::GetRegistrationsForOriginCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    std::unique_ptr<ServiceWorkerStorage::RegistrationList>
        registration_data_list,
    std::unique_ptr<std::vector<ServiceWorkerStorage::ResourceList>>
        resources_list) {
  DCHECK_EQ(registration_data_list->size(), resources_list->size());

  std::vector<storage::mojom::SerializedServiceWorkerRegistrationPtr>
      registrations;
  for (size_t i = 0; i < registration_data_list->size(); ++i) {
    registrations.push_back(
        storage::mojom::SerializedServiceWorkerRegistration::New(
            std::move((*registration_data_list)[i]),
            std::move((*resources_list)[i])));
  }

  std::move(callback).Run(status, std::move(registrations));
}

void DidGetUserDataForAllRegistrations(
    ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrationsCallback
        callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  // TODO(bashi): Change ServiceWorkerStorage::GetUserDataForAllRegistrations()
  // to return base::flat_map.
  base::flat_map<int64_t, std::string> values;
  for (auto& entry : user_data) {
    values[entry.first] = entry.second;
  }
  std::move(callback).Run(status, std::move(values));
}

}  // namespace

class ServiceWorkerLiveVersionRefImpl
    : public storage::mojom::ServiceWorkerLiveVersionRef {
 public:
  ServiceWorkerLiveVersionRefImpl(
      base::WeakPtr<ServiceWorkerStorageControlImpl> storage,
      int64_t version_id)
      : storage_(std::move(storage)), version_id_(version_id) {
    DCHECK_NE(version_id_, blink::mojom::kInvalidServiceWorkerVersionId);
    receivers_.set_disconnect_handler(
        base::BindRepeating(&ServiceWorkerLiveVersionRefImpl::OnDisconnect,
                            base::Unretained(this)));
  }
  ~ServiceWorkerLiveVersionRefImpl() override = default;

  void Add(mojo::PendingReceiver<storage::mojom::ServiceWorkerLiveVersionRef>
               receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void set_purgeable_resources(
      const std::vector<int64_t>& purgeable_resources) {
    DCHECK(purgeable_resources_.empty());
    purgeable_resources_ = purgeable_resources;
  }
  const std::vector<int64_t>& purgeable_resources() const {
    return purgeable_resources_;
  }

 private:
  void OnDisconnect() {
    if (storage_ && receivers_.empty())
      storage_->OnNoLiveVersion(version_id_);
  }

  base::WeakPtr<ServiceWorkerStorageControlImpl> storage_;
  const int64_t version_id_;
  std::vector<int64_t /*resource_id*/> purgeable_resources_;
  mojo::ReceiverSet<storage::mojom::ServiceWorkerLiveVersionRef> receivers_;
};

ServiceWorkerStorageControlImpl::ServiceWorkerStorageControlImpl(
    std::unique_ptr<ServiceWorkerStorage> storage)
    : storage_(std::move(storage)) {
  DCHECK(storage_);
}

ServiceWorkerStorageControlImpl::~ServiceWorkerStorageControlImpl() = default;

void ServiceWorkerStorageControlImpl::Bind(
    mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>
        receiver) {
  // There should be one connection at most for now because this class hasn't
  // moved to the storage service yet.
  DCHECK(receivers_.empty())
      << "ServiceWorkerStorageControl doesn't support multiple connections yet";

  receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerStorageControlImpl::OnNoLiveVersion(int64_t version_id) {
  auto it = live_versions_.find(version_id);
  DCHECK(it != live_versions_.end());
  storage_->PurgeResources(it->second->purgeable_resources());
  live_versions_.erase(it);
}

void ServiceWorkerStorageControlImpl::LazyInitializeForTest() {
  storage_->LazyInitializeForTest();
}

void ServiceWorkerStorageControlImpl::FindRegistrationForClientUrl(
    const GURL& client_url,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForClientUrl(
      client_url,
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidFindRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForScope(
    const GURL& scope,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForScope(
      scope,
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidFindRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::FindRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    FindRegistrationForClientUrlCallback callback) {
  storage_->FindRegistrationForId(
      registration_id, origin,
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidFindRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetRegistrationsForOrigin(
    const GURL& origin,
    GetRegistrationsForOriginCallback callback) {
  storage_->GetRegistrationsForOrigin(
      origin,
      base::BindOnce(&DidGetRegistrationsForOrigin, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::StoreRegistration(
    storage::mojom::ServiceWorkerRegistrationDataPtr registration,
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources,
    StoreRegistrationCallback callback) {
  storage_->StoreRegistrationData(
      std::move(registration), std::move(resources),
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidStoreRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::DeleteRegistration(
    int64_t registration_id,
    const GURL& origin,
    DeleteRegistrationCallback callback) {
  storage_->DeleteRegistration(
      registration_id, origin,
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidDeleteRegistration,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::UpdateToActiveState(
    int64_t registration_id,
    const GURL& origin,
    UpdateToActiveStateCallback callback) {
  storage_->UpdateToActiveState(registration_id, origin, std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateLastUpdateCheckTime(
    int64_t registration_id,
    const GURL& origin,
    base::Time last_update_check_time,
    UpdateLastUpdateCheckTimeCallback callback) {
  storage_->UpdateLastUpdateCheckTime(
      registration_id, origin, last_update_check_time, std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateNavigationPreloadEnabled(
    int64_t registration_id,
    const GURL& origin,
    bool enable,
    UpdateNavigationPreloadEnabledCallback callback) {
  storage_->UpdateNavigationPreloadEnabled(registration_id, origin, enable,
                                           std::move(callback));
}

void ServiceWorkerStorageControlImpl::UpdateNavigationPreloadHeader(
    int64_t registration_id,
    const GURL& origin,
    const std::string& value,
    UpdateNavigationPreloadHeaderCallback callback) {
  storage_->UpdateNavigationPreloadHeader(registration_id, origin, value,
                                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewRegistrationId(
    GetNewRegistrationIdCallback callback) {
  storage_->GetNewRegistrationId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetNewVersionId(
    GetNewVersionIdCallback callback) {
  storage_->GetNewVersionId(
      base::BindOnce(&ServiceWorkerStorageControlImpl::DidGetNewVersionId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetNewResourceId(
    GetNewResourceIdCallback callback) {
  storage_->GetNewResourceId(std::move(callback));
}

void ServiceWorkerStorageControlImpl::CreateResourceReader(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceReader> reader) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ServiceWorkerResourceReaderImpl>(
                                  storage_->CreateResponseReader(resource_id)),
                              std::move(reader));
}

void ServiceWorkerStorageControlImpl::CreateResourceWriter(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceWriter> writer) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(std::make_unique<ServiceWorkerResourceWriterImpl>(
                                  storage_->CreateResponseWriter(resource_id)),
                              std::move(writer));
}

void ServiceWorkerStorageControlImpl::CreateResourceMetadataWriter(
    int64_t resource_id,
    mojo::PendingReceiver<storage::mojom::ServiceWorkerResourceMetadataWriter>
        writer) {
  DCHECK_NE(resource_id, blink::mojom::kInvalidServiceWorkerResourceId);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ServiceWorkerResourceMetadataWriterImpl>(
          storage_->CreateResponseMetadataWriter(resource_id)),
      std::move(writer));
}

void ServiceWorkerStorageControlImpl::StoreUncommittedResourceId(
    int64_t resource_id,
    const GURL& origin,
    StoreUncommittedResourceIdCallback callback) {
  storage_->StoreUncommittedResourceId(resource_id, origin,
                                       std::move(callback));
}

void ServiceWorkerStorageControlImpl::DoomUncommittedResources(
    const std::vector<int64_t>& resource_ids,
    DoomUncommittedResourcesCallback callback) {
  storage_->DoomUncommittedResources(resource_ids, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    GetUserDataCallback callback) {
  storage_->GetUserData(registration_id, keys, std::move(callback));
}

void ServiceWorkerStorageControlImpl::StoreUserData(
    int64_t registration_id,
    const GURL& origin,
    std::vector<storage::mojom::ServiceWorkerUserDataPtr> user_data,
    StoreUserDataCallback callback) {
  storage_->StoreUserData(registration_id, origin, std::move(user_data),
                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::ClearUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    ClearUserDataCallback callback) {
  storage_->ClearUserData(registration_id, keys, std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserDataByKeyPrefixCallback callback) {
  storage_->GetUserDataByKeyPrefix(registration_id, key_prefix,
                                   std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserKeysAndDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    GetUserKeysAndDataByKeyPrefixCallback callback) {
  storage_->GetUserKeysAndDataByKeyPrefix(registration_id, key_prefix,
                                          std::move(callback));
}

void ServiceWorkerStorageControlImpl::ClearUserDataByKeyPrefixes(
    int64_t registration_id,
    const std::vector<std::string>& key_prefixes,
    ClearUserDataByKeyPrefixesCallback callback) {
  storage_->ClearUserDataByKeyPrefixes(registration_id, key_prefixes,
                                       std::move(callback));
}

void ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrations(
    const std::string& key,
    GetUserDataForAllRegistrationsCallback callback) {
  storage_->GetUserDataForAllRegistrations(
      key,
      base::BindOnce(&DidGetUserDataForAllRegistrations, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    GetUserDataForAllRegistrationsByKeyPrefixCallback callback) {
  storage_->GetUserDataForAllRegistrationsByKeyPrefix(
      key_prefix,
      base::BindOnce(&DidGetUserDataForAllRegistrations, std::move(callback)));
}

void ServiceWorkerStorageControlImpl::
    ClearUserDataForAllRegistrationsByKeyPrefix(
        const std::string& key_prefix,
        ClearUserDataForAllRegistrationsByKeyPrefixCallback callback) {
  storage_->ClearUserDataForAllRegistrationsByKeyPrefix(key_prefix,
                                                        std::move(callback));
}

void ServiceWorkerStorageControlImpl::ApplyPolicyUpdates(
    const std::vector<storage::mojom::LocalStoragePolicyUpdatePtr>
        policy_updates) {
  storage_->ApplyPolicyUpdates(std::move(policy_updates));
}

void ServiceWorkerStorageControlImpl::DidFindRegistration(
    base::OnceCallback<
        void(storage::mojom::ServiceWorkerFindRegistrationResultPtr)> callback,
    storage::mojom::ServiceWorkerRegistrationDataPtr data,
    std::unique_ptr<ResourceList> resources,
    storage::mojom::ServiceWorkerDatabaseStatus status) {
  ResourceList resource_list =
      resources ? std::move(*resources) : ResourceList();

  mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
      remote_reference;
  if (data &&
      data->version_id != blink::mojom::kInvalidServiceWorkerVersionId) {
    DCHECK_EQ(status, storage::mojom::ServiceWorkerDatabaseStatus::kOk);
    auto it = live_versions_.find(data->version_id);
    if (it == live_versions_.end()) {
      remote_reference = CreateLiveVersionReference(data->version_id);
    } else {
      it->second->Add(remote_reference.InitWithNewPipeAndPassReceiver());
    }
  }

  std::move(callback).Run(
      storage::mojom::ServiceWorkerFindRegistrationResult::New(
          status, std::move(remote_reference), std::move(data),
          std::move(resource_list)));
}

void ServiceWorkerStorageControlImpl::DidStoreRegistration(
    StoreRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    int64_t deleted_version_id,
    const std::vector<int64_t>& newly_purgeable_resources) {
  MaybePurgeResources(deleted_version_id, newly_purgeable_resources);
  std::move(callback).Run(status);
}

void ServiceWorkerStorageControlImpl::DidDeleteRegistration(
    DeleteRegistrationCallback callback,
    storage::mojom::ServiceWorkerDatabaseStatus status,
    ServiceWorkerStorage::OriginState origin_state,
    int64_t deleted_version_id,
    const std::vector<int64_t>& newly_purgeable_resources) {
  MaybePurgeResources(deleted_version_id, newly_purgeable_resources);
  std::move(callback).Run(status, origin_state);
}

void ServiceWorkerStorageControlImpl::DidGetNewVersionId(
    GetNewVersionIdCallback callback,
    int64_t version_id) {
  mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
      remote_reference;
  if (version_id != blink::mojom::kInvalidServiceWorkerVersionId) {
    remote_reference = CreateLiveVersionReference(version_id);
  }
  std::move(callback).Run(version_id, std::move(remote_reference));
}

mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
ServiceWorkerStorageControlImpl::CreateLiveVersionReference(
    int64_t version_id) {
  DCHECK_NE(version_id, blink::mojom::kInvalidServiceWorkerVersionId);
  DCHECK(!base::Contains(live_versions_, version_id));

  mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
      remote_reference;
  auto reference = std::make_unique<ServiceWorkerLiveVersionRefImpl>(
      weak_ptr_factory_.GetWeakPtr(), version_id);
  reference->Add(remote_reference.InitWithNewPipeAndPassReceiver());
  live_versions_[version_id] = std::move(reference);
  return remote_reference;
}

void ServiceWorkerStorageControlImpl::MaybePurgeResources(
    int64_t version_id,
    const std::vector<int64_t>& purgeable_resources) {
  if (version_id == blink::mojom::kInvalidServiceWorkerVersionId ||
      purgeable_resources.size() == 0)
    return;

  if (base::Contains(live_versions_, version_id)) {
    live_versions_[version_id]->set_purgeable_resources(
        std::move(purgeable_resources));
  } else {
    storage_->PurgeResources(std::move(purgeable_resources));
  }
}

}  // namespace content
