// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_

#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/test/test_renderer_host.h"

namespace performance_manager {

// A test harness that initializes PerformanceManagerImpl, plus the entire
// RenderViewHost harness. Allows for creating full WebContents, and their
// accompanying structures in the graph. The task environment is accessed
// via content::RenderViewHostTestHarness::task_environment(). RenderFrameHosts
// and such are not created, so this is suitable for unit tests but not
// browser tests.
//
// Meant to be used from components_unittests, but not from unit_tests.
//
// If you just want to test how code interacts with the graph use
// GraphTestHarness, which has a richer set of methods for creating graph
// nodes.
class PerformanceManagerTestHarness
    : public content::RenderViewHostTestHarness {
 public:
  using Super = content::RenderViewHostTestHarness;

  PerformanceManagerTestHarness();

  // Constructs a PerformanceManagerTestHarness which uses |traits| to
  // initialize its BrowserTaskEnvironment.
  template <typename... TaskEnvironmentTraits>
  explicit PerformanceManagerTestHarness(TaskEnvironmentTraits&&... traits)
      : Super(std::forward<TaskEnvironmentTraits>(traits)...) {
    helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
  }

  PerformanceManagerTestHarness(const PerformanceManagerTestHarness&) = delete;
  PerformanceManagerTestHarness& operator=(
      const PerformanceManagerTestHarness&) = delete;
  ~PerformanceManagerTestHarness() override;

  void SetUp() override;
  void TearDown() override;

  // Creates a test web contents with performance manager tab helpers
  // attached. This is a test web contents that can be interacted with
  // via WebContentsTester.
  std::unique_ptr<content::WebContents> CreateTestWebContents();

  // Allows a test to cause the PM to be torn down early, so it can explicitly
  // test TearDown logic. This may only be called once.
  void TearDownNow();

 private:
  std::unique_ptr<PerformanceManagerTestHarnessHelper> helper_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_TEST_HARNESS_H_
