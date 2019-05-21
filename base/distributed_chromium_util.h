// Copyright 2018 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DISTRIBUTED_CHROMIUM_H_
#define BASE_DISTRIBUTED_CHROMIUM_H_

#include <stddef.h>
#include <string>

#include "base/base_export.h"

namespace base {

// Castanets supports several operating systems for the browser.
enum OSType {
  ANDROID_OS = 0,
  LINUX_OS,
  TIZEN_OS,
  WINDOWS_OS,
  OTHERS,
};

class BASE_EXPORT Castanets {
  public:
    static bool IsEnabled();
    static std::string ServerAddress();
    static void SetBrowserOSType();
    static int GetBrowserOSType();
};

}  // namespace base

#endif  // BASE_DISTRIBUTED_CHROMIUM_H_
