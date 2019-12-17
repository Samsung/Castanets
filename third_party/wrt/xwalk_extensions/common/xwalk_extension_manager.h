// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_EXTENSIONS_XWALK_EXTENSION_MANAGER_H_
#define XWALK_EXTENSIONS_XWALK_EXTENSION_MANAGER_H_

#include <string>
#include <set>
#include <map>

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension.h"

namespace extensions {

class XWalkExtensionManager : public XWalkExtension::XWalkExtensionDelegate {
 public:
  typedef std::set<std::string> StringSet;
  typedef std::map<std::string, XWalkExtension*> ExtensionMap;

  XWalkExtensionManager();
  virtual ~XWalkExtensionManager();

  ExtensionMap extensions() const { return extensions_; }

  void LoadExtensions(bool meta_only = true);
  void LoadUserExtensions(const std::string app_path);
  void PreloadExtensions();

  void UnloadExtensions();

 private:
  // override
  void GetRuntimeVariable(const char* key, char* value, size_t value_len);

  bool RegisterSymbols(XWalkExtension* extension);
  void RegisterExtension(XWalkExtension* extension);
  void RegisterExtensionsByMeta(const std::string& meta_path,
                                StringSet* files);

  StringSet extension_symbols_;
  ExtensionMap extensions_;
};

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_XWALK_EXTENSION_MANAGER_H_
