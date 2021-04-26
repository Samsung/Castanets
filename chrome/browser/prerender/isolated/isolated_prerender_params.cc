// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/common/chrome_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

const char kIsolatedPrerenderEnableNSPCmdLineFlag[] =
    "isolated-prerender-nsp-enabled";

bool IsolatedPrerenderIsEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatePrerenders);
}

bool IsolatedPrerenderNoStatePrefetchSubresources() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             kIsolatedPrerenderEnableNSPCmdLineFlag) ||
         base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "do_no_state_prefetch", false);
}

base::Optional<size_t> IsolatedPrerenderMaximumNumberOfPrefetches() {
  if (!IsolatedPrerenderIsEnabled()) {
    return 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "isolated-prerender-unlimited-prefetches")) {
    return base::nullopt;
  }

  int max = base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                                   "max_srp_prefetches", 1);
  if (max < 0) {
    return base::nullopt;
  }
  return max;
}

base::Optional<size_t>
IsolatedPrerenderMaximumNumberOfNoStatePrefetchAttempts() {
  if (!IsolatedPrerenderIsEnabled() ||
      !IsolatedPrerenderNoStatePrefetchSubresources()) {
    return 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "isolated-prerender-unlimited-nsp")) {
    return base::nullopt;
  }

  int max = base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                                   "max_nsp", 1);
  if (max < 0) {
    return base::nullopt;
  }
  return max;
}

base::TimeDelta IsolatedPrerenderProbeTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          features::kIsolatePrerendersMustProbeOrigin, "probe_timeout_ms",
          10 * 1000 /* 10 seconds */));
}

bool IsolatedPrerenderCloseIdleSockets() {
  return base::GetFieldTrialParamByFeatureAsBool(features::kIsolatePrerenders,
                                                 "close_idle_sockets", true);
}

base::TimeDelta IsolatedPrefetchTimeoutDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(features::kIsolatePrerenders,
                                             "prefetch_timeout_ms",
                                             10 * 1000 /* 10 seconds */));
}

bool IsolatedPrerenderProbingEnabled() {
  return base::FeatureList::IsEnabled(
      features::kIsolatePrerendersMustProbeOrigin);
}

bool IsolatedPrerenderCanaryCheckEnabled() {
  if (!base::FeatureList::IsEnabled(
          features::kIsolatePrerendersMustProbeOrigin)) {
    return false;
  }

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kIsolatePrerendersMustProbeOrigin, "do_canary", true);
}

GURL IsolatedPrerenderCanaryCheckURL() {
  GURL url(base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerendersMustProbeOrigin, "canary_url"));
  if (url.is_valid()) {
    return url;
  }
  return GURL("http://check.googlezip.net/connect");
}

base::TimeDelta IsolatedPrerenderCanaryCheckCacheLifetime() {
  return base::TimeDelta::FromHours(base::GetFieldTrialParamByFeatureAsInt(
      features::kIsolatePrerendersMustProbeOrigin, "canary_cache_hours", 24));
}

IsolatedPrerenderOriginProbeType IsolatedPrerenderOriginProbeMechanism() {
  std::string param = base::GetFieldTrialParamValueByFeature(
      features::kIsolatePrerendersMustProbeOrigin, "probe_type");
  if (param == "dns")
    return IsolatedPrerenderOriginProbeType::kDns;
  if (param == "http_head")
    return IsolatedPrerenderOriginProbeType::kHttpHead;

  return IsolatedPrerenderOriginProbeType::kHttpHead;
}
