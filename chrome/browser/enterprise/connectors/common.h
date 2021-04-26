// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <set>
#include <string>

#include "components/enterprise/common/proto/connectors.pb.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Keys used to read a connector's policy values.
constexpr char kKeyServiceProvider[] = "service_provider";
constexpr char kKeyEnable[] = "enable";
constexpr char kKeyDisable[] = "disable";
constexpr char kKeyUrlList[] = "url_list";
constexpr char kKeyTags[] = "tags";
constexpr char kKeyBlockUntilVerdict[] = "block_until_verdict";
constexpr char kKeyBlockPasswordProtected[] = "block_password_protected";
constexpr char kKeyBlockLargeFiles[] = "block_large_files";
constexpr char kKeyBlockUnsupportedFileTypes[] = "block_unsupported_file_types";

enum class ReportingConnector {
  SECURITY_EVENT,
};

// Enum representing if an analysis should block further interactions with the
// browser until its verdict is obtained.
enum class BlockUntilVerdict {
  NO_BLOCK = 0,
  BLOCK = 1,
};

// Structs representing settings to be used for an analysis or a report. These
// settings should only be kept and considered valid for the specific
// analysis/report they were obtained for.
struct AnalysisSettings {
  AnalysisSettings();
  AnalysisSettings(AnalysisSettings&&);
  AnalysisSettings& operator=(AnalysisSettings&&);
  ~AnalysisSettings();

  GURL analysis_url;
  std::set<std::string> tags;
  BlockUntilVerdict block_until_verdict = BlockUntilVerdict::NO_BLOCK;
  bool block_password_protected_files = false;
  bool block_large_files = false;
  bool block_unsupported_file_types = false;
};

struct ReportingSettings {
  ReportingSettings();
  explicit ReportingSettings(GURL url);
  ReportingSettings(ReportingSettings&&);
  ReportingSettings& operator=(ReportingSettings&&);
  ~ReportingSettings();

  GURL reporting_url;
};

// Returns the pref path corresponding to a connector.
const char* ConnectorPref(AnalysisConnector connector);
const char* ConnectorPref(ReportingConnector connector);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
