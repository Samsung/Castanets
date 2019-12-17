// Copyright (c) 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches only for efl, or Tizen.

#ifndef CONTENT_SWITCHES_EFL_H_
#define CONTENT_SWITCHES_EFL_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace switches {

// Enables the use of view-mode CSS media feature, which allows
// pages to match the different visual presentation modes that can be applied
// to web applications and thereby apply different styling based on these
// different modes using CSS Media Queries.
CONTENT_EXPORT extern const char kEnableViewMode[];
CONTENT_EXPORT extern const char kInjectedBundlePath[];
#if defined(OS_TIZEN_TV_PRODUCT)
CONTENT_EXPORT extern const char kAllowUniversalAccessFromFiles[];
// Save ApplicationType in command line.
CONTENT_EXPORT extern const char kApplicationType[];
// CORS enabled url schemes
CONTENT_EXPORT extern const char kCORSEnabledURLSchemes[];
// Enables time offset
CONTENT_EXPORT extern const char kTimeOffset[];
// JS plugin mime types
CONTENT_EXPORT extern const char kJSPluginMimeTypes[];
// max url characters
CONTENT_EXPORT extern const char kMaxUrlCharacters[];
CONTENT_EXPORT extern const char kDiskCacheSize[];
// Allow Tizen Sockets WebAPI to be enabled in WebBrowser/ubrowser,
// which is used for development purposes.
CONTENT_EXPORT extern const char kEnableTizenSocketsWebApi[];
#endif
CONTENT_EXPORT extern const char kTizenAppId[];
CONTENT_EXPORT extern const char kWidgetScale[];
CONTENT_EXPORT extern const char kWidgetTheme[];
CONTENT_EXPORT extern const char kWidgetEncodedBundle[];
CONTENT_EXPORT extern const char kTizenAppVersion[];

// Turns on a bunch of settings (mostly on blink::WebView) for which there is no
// command line switches. This allows desktop "ubrowser --mobile" to have
// similar set of features that mobile build has.
CONTENT_EXPORT extern const char kEwkEnableMobileFeaturesForDesktop[];

CONTENT_EXPORT extern const char kLimitMemoryAllocationInScheduleDelayedWork[];

#if defined(TIZEN_PEPPER_EXTENSIONS)
// Allow Trusted Pepper Plugins to be loaded in WebBrowser/ubrowser,
// which is used for development purposes.
CONTENT_EXPORT extern const char kEnableTrustedPepperPlugins[];

// Additional Trusted Pepper Plugins search paths.
// Useful for development purposes. Multiple files can be used by separating
// them with a semicolon (;).
CONTENT_EXPORT extern const char kTrustedPepperPluginsSearchPaths[];
#endif
// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CONTENT_SWITCHES_EFL_H_
