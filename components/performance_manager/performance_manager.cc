// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/performance_manager.h"

#include <utility>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/public/performance_manager_owned.h"

namespace performance_manager {

PerformanceManager::PerformanceManager() = default;
PerformanceManager::~PerformanceManager() = default;

// static
void PerformanceManager::CallOnGraph(const base::Location& from_here,
                                     base::OnceClosure callback) {
  DCHECK(callback);

  PerformanceManagerImpl::GetTaskRunner()->PostTask(from_here,
                                                    std::move(callback));
}
// static
void PerformanceManager::CallOnGraph(const base::Location& from_here,
                                     GraphCallback callback) {
  DCHECK(callback);

  // TODO(siggi): Unwrap this by binding the loose param.
  PerformanceManagerImpl::GetTaskRunner()->PostTask(
      from_here, base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraph,
                                std::move(callback)));
}

// static
void PerformanceManager::PassToGraph(const base::Location& from_here,
                                     std::unique_ptr<GraphOwned> graph_owned) {
  DCHECK(graph_owned);

  // PassToGraph() should only be called when a graph is available to take
  // ownership of |graph_owned|.
  DCHECK(IsAvailable());

  PerformanceManagerImpl::CallOnGraphImpl(
      from_here,
      base::BindOnce(
          [](std::unique_ptr<GraphOwned> graph_owned, GraphImpl* graph) {
            graph->PassToGraph(std::move(graph_owned));
          },
          std::move(graph_owned)));
}

// static
base::WeakPtr<PageNode> PerformanceManager::GetPageNodeForWebContents(
    content::WebContents* wc) {
  DCHECK(wc);
  PerformanceManagerTabHelper* helper =
      PerformanceManagerTabHelper::FromWebContents(wc);
  if (!helper)
    return nullptr;
  return helper->page_node()->GetWeakPtr();
}

// static
base::WeakPtr<FrameNode> PerformanceManager::GetFrameNodeForRenderFrameHost(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  auto* wc = content::WebContents::FromRenderFrameHost(rfh);
  PerformanceManagerTabHelper* helper =
      PerformanceManagerTabHelper::FromWebContents(wc);
  if (!helper)
    return nullptr;

  return helper->GetFrameNode(rfh)->GetWeakPtr();
}

// static
void PerformanceManager::AddObserver(
    PerformanceManagerMainThreadObserver* observer) {
  PerformanceManagerRegistryImpl::GetInstance()->AddObserver(observer);
}

// static
void PerformanceManager::RemoveObserver(
    PerformanceManagerMainThreadObserver* observer) {
  PerformanceManagerRegistryImpl::GetInstance()->RemoveObserver(observer);
}

// static
void PerformanceManager::AddMechanism(
    PerformanceManagerMainThreadMechanism* mechanism) {
  PerformanceManagerRegistryImpl::GetInstance()->AddMechanism(mechanism);
}

// static
void PerformanceManager::RemoveMechanism(
    PerformanceManagerMainThreadMechanism* mechanism) {
  PerformanceManagerRegistryImpl::GetInstance()->RemoveMechanism(mechanism);
}

// static
bool PerformanceManager::HasMechanism(
    PerformanceManagerMainThreadMechanism* mechanism) {
  return PerformanceManagerRegistryImpl::GetInstance()->HasMechanism(mechanism);
}

// static
void PerformanceManager::PassToPM(
    std::unique_ptr<PerformanceManagerOwned> pm_owned) {
  return PerformanceManagerRegistryImpl::GetInstance()->PassToPM(
      std::move(pm_owned));
}

// static
std::unique_ptr<PerformanceManagerOwned> PerformanceManager::TakeFromPM(
    PerformanceManagerOwned* pm_owned) {
  return PerformanceManagerRegistryImpl::GetInstance()->TakeFromPM(pm_owned);
}

// static
void PerformanceManager::RegisterObject(
    PerformanceManagerRegistered* pm_object) {
  return PerformanceManagerRegistryImpl::GetInstance()->RegisterObject(
      pm_object);
}

// static
void PerformanceManager::UnregisterObject(
    PerformanceManagerRegistered* pm_object) {
  return PerformanceManagerRegistryImpl::GetInstance()->UnregisterObject(
      pm_object);
}

// static
PerformanceManagerRegistered* PerformanceManager::GetRegisteredObject(
    uintptr_t type_id) {
  return PerformanceManagerRegistryImpl::GetInstance()->GetRegisteredObject(
      type_id);
}

// static
scoped_refptr<base::SequencedTaskRunner> PerformanceManager::GetTaskRunner() {
  return PerformanceManagerImpl::GetTaskRunner();
}

}  // namespace performance_manager
