// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_manager.h"

#include <glob.h>
#include <dlfcn.h>
#include <sys/utsname.h>

#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
// #include "third_party/wrt/common/app_db.h"
// #include "third_party/wrt/common/file_utils.h"
// #include "third_party/wrt/common/picojson.h"
#include "third_party/wrt/common/string_utils.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension.h"
#include "third_party/wrt/xwalk_extensions/common/xwalk_extension_constants.h"

#define XWALK_EXTENSION_PATH ""

#ifndef XWALK_EXTENSION_PATH
  #error XWALK_EXTENSION_PATH is not set.
#endif

namespace extensions {

namespace {

const char kAppDBRuntimeSection[] = "Runtime";

const char kExtensionPrefix[] = "lib";
const char kExtensionSuffix[] = ".so";
const char kExtensionMetadataSuffix[] = ".json";

static const char* kPreloadLibs[] = {
  XWALK_EXTENSION_PATH"/libtizen.so",
  XWALK_EXTENSION_PATH"/libtizen_common.so",
  XWALK_EXTENSION_PATH"/libtizen_application.so",
  XWALK_EXTENSION_PATH"/libtizen_utils.so",
  NULL
};

const char kUserPluginsDirectory[] = "plugin/";
const char kArchArmv7l[] = "armv7l";
const char kArchI586[] = "i586";
const char kArchDefault[] = "default";

}  // namespace

XWalkExtensionManager::XWalkExtensionManager() {
}

XWalkExtensionManager::~XWalkExtensionManager() {
}

void XWalkExtensionManager::PreloadExtensions() {
  for (int i = 0; kPreloadLibs[i]; i++) {
    LOG(INFO) << "Preload libs : " << kPreloadLibs[i];
    void* handle = dlopen(kPreloadLibs[i], RTLD_NOW|RTLD_GLOBAL);
    if (handle == nullptr) {
      LOG(WARNING) << "Fail to load libs : " << dlerror();
    }
  }
}

void XWalkExtensionManager::LoadExtensions(bool meta_only) {
  if (!extensions_.empty()) {
    return;
  }
  /*
  std::string extension_path(XWALK_EXTENSION_PATH);

  // Gets all extension files in the XWALK_EXTENSION_PATH
  std::string ext_pattern(extension_path);
  ext_pattern.append("/");
  ext_pattern.append(kExtensionPrefix);
  ext_pattern.append("*");
  ext_pattern.append(kExtensionSuffix);

  StringSet files;
  {
    glob_t glob_result;
    glob(ext_pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
      files.insert(glob_result.gl_pathv[i]);
    }
  }

  // Gets all metadata files in the XWALK_EXTENSION_PATH
  // Loads information from the metadata files and remove the loaded file from
  // the set 'files'
  std::string meta_pattern(extension_path);
  meta_pattern.append("/");
  meta_pattern.append("*");
  meta_pattern.append(kExtensionMetadataSuffix);
  {
    glob_t glob_result;
    glob(meta_pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
      RegisterExtensionsByMeta(glob_result.gl_pathv[i], &files);
    }
  }

  // Load extensions in the remained files of the set 'files'
  if (!meta_only) {
    for (auto it = files.begin(); it != files.end(); ++it) {
      XWalkExtension* ext = new XWalkExtension(*it, this);
      RegisterExtension(ext);
    }
  }
  */
}

void XWalkExtensionManager::LoadUserExtensions(const std::string app_path) {
  if (app_path.empty()) {
    LOG(ERROR) << "Failed to get package root path";
    return;
  }
  /*
  std::string app_ext_pattern(app_path);
  app_ext_pattern.append(kUserPluginsDirectory);
  struct utsname u;
  if (0 == uname(&u)) {
    std::string machine = u.machine;
    if (!machine.empty()) {
      if (machine == kArchArmv7l) {
        app_ext_pattern.append(kArchArmv7l);
      } else if (machine == kArchI586) {
        app_ext_pattern.append(kArchI586);
      } else {
        app_ext_pattern.append(kArchDefault);
      }
    } else {
      LOG(ERROR) << "cannot get machine info";
      app_ext_pattern.append(kArchDefault);
    }
    app_ext_pattern.append("/");
  }
  app_ext_pattern.append("*");
  app_ext_pattern.append(kExtensionSuffix);

  StringSet files;
  {
    glob_t glob_result;
    glob(app_ext_pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
      files.insert(glob_result.gl_pathv[i]);
    }
  }
  for (auto it = files.begin(); it != files.end(); ++it) {
    XWalkExtension* ext = new XWalkExtension(*it, this);
    RegisterExtension(ext);
  }
  LOG(INFO) << "finish load user extension plugins";
  */
}

void XWalkExtensionManager::UnloadExtensions() {
  for (auto it = extensions_.begin(); it != extensions_.end(); ++it) {
    delete it->second;
  }
  extensions_.clear();
}

bool XWalkExtensionManager::RegisterSymbols(XWalkExtension* extension) {
  std::string name = extension->name();

  if (extension_symbols_.find(name) != extension_symbols_.end()) {
    LOG(WARNING) << "Ignoring extension with name already registred. '" << name
                 << "'";
    return false;
  }

  XWalkExtension::StringVector entry_points = extension->entry_points();
  for (auto it = entry_points.begin(); it != entry_points.end(); ++it) {
    if (extension_symbols_.find(*it) != extension_symbols_.end()) {
      LOG(WARNING) << "Ignoring extension with entry_point already registred. '"
                   << (*it) << "'";
      return false;
    }
  }

  for (auto it = entry_points.begin(); it != entry_points.end(); ++it) {
    extension_symbols_.insert(*it);
  }

  extension_symbols_.insert(name);

  return true;
}

void XWalkExtensionManager::RegisterExtension(XWalkExtension* extension) {
  if (!extension->lazy_loading() && !extension->Initialize()) {
    delete extension;
    return;
  }

  if (!RegisterSymbols(extension)) {
    delete extension;
    return;
  }

  extensions_[extension->name()] = extension;
  LOG(INFO) << extension->name() << " is registered.";
}

void XWalkExtensionManager::RegisterExtensionsByMeta(
    const std::string& meta_path, StringSet* files) {
  /*
  std::string extension_path(XWALK_EXTENSION_PATH);

  std::ifstream metafile(meta_path.c_str());
  if (!metafile.is_open()) {
    LOG(ERROR) << "Fail to open the plugin metadata file :" << meta_path;
    return;
  }

  picojson::value metadata;
  metafile >> metadata;
    if (metadata.is<picojson::array>()) {
    auto& plugins = metadata.get<picojson::array>();
    for (auto plugin = plugins.begin(); plugin != plugins.end(); ++plugin) {
      if (!plugin->is<picojson::object>())
        continue;

      std::string name = plugin->get("name").to_str();
      std::string lib = plugin->get("lib").to_str();
      if (!common::utils::StartsWith(lib, "/")) {
        lib = extension_path + "/" + lib;
      }

      std::vector<std::string> entries;
      auto& entry_points_value = plugin->get("entry_points");
      if (entry_points_value.is<picojson::array>()) {
        auto& entry_points = entry_points_value.get<picojson::array>();
        for (auto entry = entry_points.begin(); entry != entry_points.end();
             ++entry) {
          entries.push_back(entry->to_str());
        }
      }
      XWalkExtension* extension = new XWalkExtension(lib, name, entries, this);
      RegisterExtension(extension);
      files->erase(lib);
    }
  } else {
    LOG(ERROR) << meta_path << " is not a valid metadata file.";
  }
  metafile.close();
  */
}

// override
void XWalkExtensionManager::GetRuntimeVariable(
    const char* key, char* value, size_t value_len) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  std::string ret = db->Get(kAppDBRuntimeSection, key);
  strncpy(value, ret.c_str(), value_len);
  */
}


}  // namespace extensions
