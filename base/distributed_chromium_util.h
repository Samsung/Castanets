// Copyright 2020 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DISTRIBUTED_CHROMIUM_H_
#define BASE_DISTRIBUTED_CHROMIUM_H_

#include <stddef.h>
#include <string>

#include "base/base_export.h"

namespace base {
#if defined(CASTANETS)
class BASE_EXPORT Castanets {
 public:
  static bool IsEnabled();
  static std::string ServerAddress();
};
#endif

#if defined(SERVICE_OFFLOADING)
class BASE_EXPORT ServiceOffloading {
 public:
  static bool IsEnabled();
};
#endif

}  // namespace base

#endif  // BASE_DISTRIBUTED_CHROMIUM_H_
