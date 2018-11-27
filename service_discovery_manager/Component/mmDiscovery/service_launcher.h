/*
 * Copyright 2018 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __INCLUDE_SERVICE_LAUNCHER_H__
#define __INCLUDE_SERVICE_LAUNCHER_H__

#include <sys/types.h>
#include <vector>

class ServiceLauncher {
 public:
  ServiceLauncher(const char* path) { chromium_path_ = path; }
  ~ServiceLauncher() {}

  unsigned ActivatedRendererCount();
  bool LaunchRenderer(std::vector<char*>& argv);

 private:
  const char* chromium_path_;
  std::vector<pid_t> children_;
};

#endif
