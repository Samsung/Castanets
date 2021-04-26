// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_

// Utility functions for App Service intent handling.

#include <string>

#include "base/macros.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "url/gurl.h"

namespace apps_util {

extern const char kIntentActionView[];
extern const char kIntentActionSend[];
extern const char kIntentActionSendMultiple[];

// Create an intent struct from URL.
apps::mojom::IntentPtr CreateIntentFromUrl(const GURL& url);

// Return true if |value| matches with the |condition_value|, based on the
// pattern match type in the |condition_value|.
bool ConditionValueMatches(
    const std::string& value,
    const apps::mojom::ConditionValuePtr& condition_value);

// Return true if |intent| matches with any of the values in |condition|.
bool IntentMatchesCondition(const apps::mojom::IntentPtr& intent,
                            const apps::mojom::ConditionPtr& condition);

// Return true if a |filter| matches an |intent|. This is true when intent
// matches all existing conditions in the filter.
bool IntentMatchesFilter(const apps::mojom::IntentPtr& intent,
                         const apps::mojom::IntentFilterPtr& filter);

// Return true if |value| matches |pattern| with simple glob syntax.
// In this syntax, you can use the '*' character to match against zero or
// more occurrences of the character immediately before. If the character
// before it is '.' it will match any character. The character '\' can be
// used as an escape. This essentially provides only the '*' wildcard part
// of a normal regexp.
// This function is transcribed from android's PatternMatcher#matchPattern.
// See
// https://android.googlesource.com/platform/frameworks/base.git/+/e93165456c3c28278f275566bd90bfbcf1a0e5f7/core/java/android/os/PatternMatcher.java#186
bool MatchGlob(const std::string& value, const std::string& pattern);
}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_
