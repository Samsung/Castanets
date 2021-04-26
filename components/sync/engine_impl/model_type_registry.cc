// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_registry.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"
#include "components/sync/engine_impl/model_type_worker.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/syncable_base_transaction.h"

namespace syncer {

namespace {

class CommitQueueProxy : public CommitQueue {
 public:
  CommitQueueProxy(const base::WeakPtr<CommitQueue>& worker,
                   const scoped_refptr<base::SequencedTaskRunner>& sync_thread);
  ~CommitQueueProxy() override;

  void NudgeForCommit() override;

 private:
  base::WeakPtr<CommitQueue> worker_;
  scoped_refptr<base::SequencedTaskRunner> sync_thread_;
};

CommitQueueProxy::CommitQueueProxy(
    const base::WeakPtr<CommitQueue>& worker,
    const scoped_refptr<base::SequencedTaskRunner>& sync_thread)
    : worker_(worker), sync_thread_(sync_thread) {}

CommitQueueProxy::~CommitQueueProxy() {}

void CommitQueueProxy::NudgeForCommit() {
  sync_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&CommitQueue::NudgeForCommit, worker_));
}

}  // namespace

ModelTypeRegistry::ModelTypeRegistry(
    const std::vector<scoped_refptr<ModelSafeWorker>>& workers,
    UserShare* user_share,
    NudgeHandler* nudge_handler,
    CancelationSignal* cancelation_signal,
    KeystoreKeysHandler* keystore_keys_handler)
    : user_share_(user_share),
      nudge_handler_(nudge_handler),
      cancelation_signal_(cancelation_signal),
      keystore_keys_handler_(keystore_keys_handler) {
  for (size_t i = 0u; i < workers.size(); ++i) {
    workers_map_.insert(
        std::make_pair(workers[i]->GetModelSafeGroup(), workers[i]));
  }
}

ModelTypeRegistry::~ModelTypeRegistry() = default;

void ModelTypeRegistry::ConnectNonBlockingType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(!IsProxyType(type));
  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());
  DVLOG(1) << "Enabling an off-thread sync type: " << ModelTypeToString(type);

  // Save a raw pointer to the processor for connecting later.
  ModelTypeProcessor* type_processor =
      activation_response->type_processor.get();

  std::unique_ptr<Cryptographer> cryptographer_copy;
  if (encrypted_types_.Has(type))
    cryptographer_copy = cryptographer_->Clone();

  DataTypeDebugInfoEmitter* emitter = GetEmitter(type);
  if (emitter == nullptr) {
    auto new_emitter = std::make_unique<NonBlockingTypeDebugInfoEmitter>(
        type, &type_debug_info_observers_);
    emitter = new_emitter.get();
    data_type_debug_info_emitter_map_.insert(
        std::make_pair(type, std::move(new_emitter)));
  }

  bool initial_sync_done =
      activation_response->model_type_state.initial_sync_done();
  auto worker = std::make_unique<ModelTypeWorker>(
      type, activation_response->model_type_state,
      /*trigger_initial_sync=*/!initial_sync_done,
      std::move(cryptographer_copy), passphrase_type_, nudge_handler_,
      std::move(activation_response->type_processor), emitter,
      cancelation_signal_);

  // Save a raw pointer and add the worker to our structures.
  ModelTypeWorker* worker_ptr = worker.get();
  model_type_workers_.push_back(std::move(worker));
  update_handler_map_.insert(std::make_pair(type, worker_ptr));
  commit_contributor_map_.insert(std::make_pair(type, worker_ptr));

  // Initialize Processor -> Worker communication channel.
  type_processor->ConnectSync(std::make_unique<CommitQueueProxy>(
      worker_ptr->AsWeakPtr(), base::SequencedTaskRunnerHandle::Get()));

  // If there is still data for this type left in the directory, purge it now.
  // TODO(crbug.com/1084499): The purge should be safe to do even if the initial
  // USS sync has already happened, and also for NIGORI.
  if (!initial_sync_done && directory()->InitialSyncEndedForType(type) &&
      type != NIGORI) {
    directory()->PurgeEntriesWithTypeIn(/*disabled_types=*/ModelTypeSet(type),
                                        /*types_to_journal=*/ModelTypeSet(),
                                        /*types_to_unapply=*/ModelTypeSet());
  }
}

void ModelTypeRegistry::DisconnectNonBlockingType(ModelType type) {
  DVLOG(1) << "Disabling an off-thread sync type: " << ModelTypeToString(type);

  DCHECK(!IsProxyType(type));
  DCHECK(update_handler_map_.find(type) != update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) != commit_contributor_map_.end());

  size_t updaters_erased = update_handler_map_.erase(type);
  size_t committers_erased = commit_contributor_map_.erase(type);

  DCHECK_EQ(1U, updaters_erased);
  DCHECK_EQ(1U, committers_erased);

  auto iter = model_type_workers_.begin();
  while (iter != model_type_workers_.end()) {
    if ((*iter)->GetModelType() == type) {
      iter = model_type_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void ModelTypeRegistry::ConnectProxyType(ModelType type) {
  DCHECK(IsProxyType(type));
  enabled_proxy_types_.Put(type);
}

void ModelTypeRegistry::DisconnectProxyType(ModelType type) {
  DCHECK(IsProxyType(type));
  enabled_proxy_types_.Remove(type);
}

ModelTypeSet ModelTypeRegistry::GetEnabledTypes() const {
  return Union(GetEnabledNonBlockingTypes(), enabled_proxy_types_);
}

ModelTypeSet ModelTypeRegistry::GetInitialSyncEndedTypes() const {
  ModelTypeSet result;
  for (const auto& kv : update_handler_map_) {
    if (kv.second->IsInitialSyncEnded())
      result.Put(kv.first);
  }
  return result;
}

const UpdateHandler* ModelTypeRegistry::GetUpdateHandler(ModelType type) const {
  auto it = update_handler_map_.find(type);
  return it == update_handler_map_.end() ? nullptr : it->second;
}

UpdateHandlerMap* ModelTypeRegistry::update_handler_map() {
  return &update_handler_map_;
}

CommitContributorMap* ModelTypeRegistry::commit_contributor_map() {
  return &commit_contributor_map_;
}

KeystoreKeysHandler* ModelTypeRegistry::keystore_keys_handler() {
  return keystore_keys_handler_;
}

void ModelTypeRegistry::RegisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  if (!type_debug_info_observers_.HasObserver(observer))
    type_debug_info_observers_.AddObserver(observer);
}

void ModelTypeRegistry::UnregisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  type_debug_info_observers_.RemoveObserver(observer);
}

bool ModelTypeRegistry::HasDirectoryTypeDebugInfoObserver(
    const TypeDebugInfoObserver* observer) const {
  return type_debug_info_observers_.HasObserver(observer);
}

void ModelTypeRegistry::RequestEmitDebugInfo() {
  for (const auto& kv : data_type_debug_info_emitter_map_) {
    kv.second->EmitCommitCountersUpdate();
    kv.second->EmitUpdateCountersUpdate();
    // Although this breaks encapsulation, don't emit status counters here.
    // They've already been asked for manually on the UI thread because USS
    // emitters don't have a working implementation yet.
  }
}

bool ModelTypeRegistry::HasUnsyncedItems() const {
  // For model type workers, we ask them individually.
  for (const auto& worker : model_type_workers_) {
    if (worker->HasLocalChangesForTest()) {
      return true;
    }
  }

  // Verify directory state.
  ReadTransaction trans(FROM_HERE, user_share_);
  return trans.GetWrappedTrans()->directory()->unsynced_entity_count() != 0;
}

base::WeakPtr<ModelTypeConnector> ModelTypeRegistry::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ModelTypeRegistry::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {}

void ModelTypeRegistry::OnPassphraseAccepted() {
  for (const auto& worker : model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->EncryptionAcceptedMaybeApplyUpdates();
    }
  }
}

void ModelTypeRegistry::OnTrustedVaultKeyRequired() {}

void ModelTypeRegistry::OnTrustedVaultKeyAccepted() {
  for (const auto& worker : model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->EncryptionAcceptedMaybeApplyUpdates();
    }
  }
}

void ModelTypeRegistry::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {}

void ModelTypeRegistry::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                                bool encrypt_everything) {
  // TODO(skym): This does not handle reducing the number of encrypted types
  // correctly. They're removed from |encrypted_types_| but corresponding
  // workers never have their Cryptographers removed. This probably is not a use
  // case that currently needs to be supported, but it should be guarded against
  // here.
  encrypted_types_ = encrypted_types;
  OnEncryptionStateChanged();
}

void ModelTypeRegistry::OnEncryptionComplete() {}

void ModelTypeRegistry::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  cryptographer_ = cryptographer->Clone();
  OnEncryptionStateChanged();
}

void ModelTypeRegistry::OnPassphraseTypeChanged(PassphraseType type,
                                                base::Time passphrase_time) {
  passphrase_type_ = type;
  for (const auto& worker : model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->UpdatePassphraseType(type);
    }
  }
}

void ModelTypeRegistry::OnEncryptionStateChanged() {
  for (const auto& worker : model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->UpdateCryptographer(cryptographer_->Clone());
    }
  }
}

DataTypeDebugInfoEmitter* ModelTypeRegistry::GetEmitter(ModelType type) {
  DataTypeDebugInfoEmitter* raw_emitter = nullptr;
  auto it = data_type_debug_info_emitter_map_.find(type);
  if (it != data_type_debug_info_emitter_map_.end()) {
    raw_emitter = it->second.get();
  }
  return raw_emitter;
}

ModelTypeSet ModelTypeRegistry::GetEnabledNonBlockingTypes() const {
  ModelTypeSet enabled_non_blocking_types;
  for (const auto& worker : model_type_workers_) {
    enabled_non_blocking_types.Put(worker->GetModelType());
  }
  return enabled_non_blocking_types;
}

}  // namespace syncer
