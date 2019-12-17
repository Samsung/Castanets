// Copyright (c) 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/content_switches_efl.h"

namespace switches {

// Enables the use of view-mode CSS media feature, which allows
// pages to match the different visual presentation modes that can be applied
// to web applications and thereby apply different styling based on these
// different modes using CSS Media Queries.
const char kEnableViewMode[]    = "enable-view-mode";
const char kInjectedBundlePath[] = "injected-bundle-path";

#if defined(OS_TIZEN_TV_PRODUCT)
// Grants universal access to origins which belongs to file:/// schema.
const char kAllowUniversalAccessFromFiles[] =
    "allow-universal-access-from-files";
// Save ApplicationType in command line.
const char kApplicationType[] = "application-type";
// CORS enabled URL schemes
const char kCORSEnabledURLSchemes[] = "cors-enabled-url-schemes";
// Time offset
const char kTimeOffset[] = "time-offset";
// JS plugin mime types
const char kJSPluginMimeTypes[] = "jsplugin-mime-types";
// max url characters
const char kMaxUrlCharacters[] = "max-url-characters";
// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[] = "disk-cache-size";
// Allow Tizen Sockets WebAPI to be enabled in WebBrowser/ubrowser,
// which is used for development purposes.
const char kEnableTizenSocketsWebApi[] = "enable-tizen-sockets-webapi";
#endif

// Widget Info
const char kTizenAppId[] = "widget-id";
const char kWidgetScale[] = "widget-scale";
const char kWidgetTheme[] = "widget-theme";
const char kWidgetEncodedBundle[] = "widget-encoded-bundle";
const char kTizenAppVersion[] = "app-version";

const char kEwkEnableMobileFeaturesForDesktop[] =
    "ewk-enable-mobile-features-for-desktop";

const char kLimitMemoryAllocationInScheduleDelayedWork[] = "limit-memory-allocation-in-schedule-delayed-work";

#if defined(TIZEN_PEPPER_EXTENSIONS)
const char kEnableTrustedPepperPlugins[] = "enable-trusted-pepper-plugins";
const char kTrustedPepperPluginsSearchPaths[] =
    "trusted-pepper-plugins-search-paths";
#endif
// Don't dump stuff here, follow the same order as the header.

}  // namespace switches
