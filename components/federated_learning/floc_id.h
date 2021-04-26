// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_

#include "base/optional.h"

#include <stdint.h>

#include <string>
#include <unordered_set>

namespace federated_learning {

// ID used to represent a cohort of people with similar browsing habits. For
// more context, see the explainer at
// https://github.com/jkarlin/floc/blob/master/README.md
class FlocId {
 public:
  static FlocId CreateFromHistory(
      const std::unordered_set<std::string>& domains);

  FlocId();
  explicit FlocId(uint64_t id);
  FlocId(const FlocId& id);

  ~FlocId();
  FlocId& operator=(const FlocId& id);
  FlocId& operator=(FlocId&& id);

  bool IsValid() const;
  uint64_t ToUint64() const;

  std::string ToDebugHeaderValue() const;

 private:
  std::string ToHeaderValue() const;

  base::Optional<uint64_t> id_;
};

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
