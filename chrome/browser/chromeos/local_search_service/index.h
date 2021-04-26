// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/local_search_service/search_metrics_reporter.h"
#include "chrome/browser/chromeos/local_search_service/shared_structs.h"

namespace local_search_service {

// A local search service Index.
// It is the client-facing API for search and indexing. It can be implemented
// with different backends that provide actual data storage/indexing/search
// functions.
class Index {
 public:
  Index(IndexId index_id, Backend backend);
  virtual ~Index();

  Index(const Index&) = delete;
  Index& operator=(const Index&) = delete;

  // Returns number of data items.
  virtual uint64_t GetSize() = 0;

  // Adds or updates data.
  // IDs of data should not be empty.
  virtual void AddOrUpdate(const std::vector<Data>& data) = 0;

  // Deletes data with |ids| and returns number of items deleted.
  // If an id doesn't exist in the Index, no operation will be done.
  // IDs should not be empty.
  virtual uint32_t Delete(const std::vector<std::string>& ids) = 0;

  // Returns matching results for a given query.
  // Zero |max_results| means no max.
  // Search behaviour depends on the implementation.
  virtual ResponseStatus Find(const base::string16& query,
                              uint32_t max_results,
                              std::vector<Result>* results) = 0;

  // Logs daily search metrics if |reporter_| is non-null and other
  // UMA metrics if |histogram_prefix_| is non-empty.
  // Each implementation of this class should call this method at the end of
  // Find.
  void MaybeLogSearchResultsStats(ResponseStatus status, size_t num_results);

  void SetSearchParams(const SearchParams& search_params);
  SearchParams GetSearchParamsForTesting();

 protected:
  SearchParams search_params_;

 private:
  std::string histogram_prefix_;
  std::unique_ptr<SearchMetricsReporter> reporter_;
  base::WeakPtrFactory<Index> weak_ptr_factory_{this};
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_
