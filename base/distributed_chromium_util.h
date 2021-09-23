// Copyright 2020 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DISTRIBUTED_CHROMIUM_H_
#define BASE_DISTRIBUTED_CHROMIUM_H_

#include <stddef.h>
#include <string>

#include "base/base_export.h"

namespace base {
class BASE_EXPORT Castanets {
 public:
  static bool IsEnabled();
  static std::string ServerAddress();
};

}  // namespace base

#endif  // BASE_DISTRIBUTED_CHROMIUM_H_
