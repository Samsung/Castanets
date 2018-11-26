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

#include "service_launcher.h"

#include <cstdlib>
#include <cstdio>
#include <unistd.h>

#include "Debugger.h"

unsigned ServiceLauncher::ActivatedRendererCount() {
  return children_.size();
}

bool ServiceLauncher::LaunchRenderer(std::vector<char*>& argv) {
  DPRINT(COMM, DEBUG_INFO, "LaunchRenderer\n");
  pid_t pid = fork();
  if (pid == -1)
    return false;

  if (pid == 0) {
    argv[0] = const_cast<char*>(chromium_path_);
    execv(argv[0], argv.data());
  }

  children_.push_back(pid);

  return true;
}
