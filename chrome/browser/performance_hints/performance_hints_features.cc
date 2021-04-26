// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_features.h"

#include "base/metrics/field_trial_params.h"

const base::Feature kPerformanceHintsObserver{
    "PerformanceHintsObserver", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPerformanceHintsTreatUnknownAsFast{
    "PerformanceHintsTreatUnknownAsFast", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPerformanceHintsHandleRewrites{
    "PerformanceHintsHandleRewrites", base::FEATURE_ENABLED_BY_DEFAULT};
constexpr base::FeatureParam<std::string> kRewriteConfig{
    &kPerformanceHintsHandleRewrites, "rewrite_config",
    "www.google.com/url?url"};

constexpr base::FeatureParam<bool> kUseFastHostHints{
    &kPerformanceHintsObserver, "use_fast_host_hints", true};

const base::Feature kContextMenuPerformanceInfo{
    "ContextMenuPerformanceInfo", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kContextMenuPerformanceInfoAndRemoteHintFetching{
    "ContextMenuPerformanceInfoAndRemoteHintFetching",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPageInfoPerformanceHints{
    "PageInfoPerformanceHints", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsPerformanceHintsObserverEnabled() {
  return base::FeatureList::IsEnabled(kPageInfoPerformanceHints) ||
         IsContextMenuPerformanceInfoEnabled() ||
         base::FeatureList::IsEnabled(kPerformanceHintsObserver);
}

bool ShouldTreatUnknownAsFast() {
  return base::FeatureList::IsEnabled(kPerformanceHintsTreatUnknownAsFast);
}

bool ShouldHandleRewrites() {
  return base::FeatureList::IsEnabled(kPerformanceHintsHandleRewrites);
}

std::string GetRewriteConfigString() {
  return kRewriteConfig.Get();
}

bool AreFastHostHintsEnabled() {
  return kUseFastHostHints.Get();
}

bool IsContextMenuPerformanceInfoEnabled() {
  return base::FeatureList::IsEnabled(kContextMenuPerformanceInfo) ||
         base::FeatureList::IsEnabled(
             kContextMenuPerformanceInfoAndRemoteHintFetching);
}

bool IsRemoteFetchingExplicitlyAllowedForPerformanceInfo() {
  return base::FeatureList::IsEnabled(
      kContextMenuPerformanceInfoAndRemoteHintFetching);
}
