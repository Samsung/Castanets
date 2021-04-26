// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_

#include <stdint.h>

#include "base/optional.h"
#include "base/time/time.h"
#include "url/gurl.h"

// This command line flag enables NoStatePrefetch on Isolated Prerenders.
extern const char kIsolatedPrerenderEnableNSPCmdLineFlag[];

// Returns true if the Isolated Prerender feature is enabled.
bool IsolatedPrerenderIsEnabled();

// Returns true when prefetched pages should run no state prefetch.
bool IsolatedPrerenderNoStatePrefetchSubresources();

// The maximum number of prefetches that should be done from predictions on a
// Google SRP. nullopt is returned for unlimited. Negative values given by the
// field trial return nullopt.
base::Optional<size_t> IsolatedPrerenderMaximumNumberOfPrefetches();

// The maximum number of no state prefetches to attempt, in order to prefetch
// the pages' subresources, while the user is on the SRP. nullopt is returned
// for unlimited. Negative values given by the field trial return nullopt.
base::Optional<size_t>
IsolatedPrerenderMaximumNumberOfNoStatePrefetchAttempts();

// Whether idle sockets should be closed after every prefetch.
bool IsolatedPrerenderCloseIdleSockets();

// The amount of time to allow before timing out an origin probe.
base::TimeDelta IsolatedPrerenderProbeTimeout();

// The amount of time to allow a prefetch to take before considering it a
// timeout error.
base::TimeDelta IsolatedPrefetchTimeoutDuration();

// Whether probing must be done at all.
bool IsolatedPrerenderProbingEnabled();

// Whether an ISP filtering canary check should be made on browser startup.
bool IsolatedPrerenderCanaryCheckEnabled();

// The URL to use for the canary check.
GURL IsolatedPrerenderCanaryCheckURL();

// How long a canary check can be cached for the same network.
base::TimeDelta IsolatedPrerenderCanaryCheckCacheLifetime();

// The type of probe that needs to be done before prefetched resources can be
// used.
enum class IsolatedPrerenderOriginProbeType {
  kDns,
  kHttpHead,
};
IsolatedPrerenderOriginProbeType IsolatedPrerenderOriginProbeMechanism();

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
