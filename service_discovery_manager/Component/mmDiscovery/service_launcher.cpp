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

#ifdef WIN32

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif



#include <cstdlib>
#include <cstdio>

#ifdef WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "Debugger.h"
#include "processAPI.h"

#include "service_launcher.h"

static const char kProcSelfExe[] = "/proc/self/exe";

unsigned ServiceLauncher::ActivatedRendererCount() {
  return children_.size();
}

bool ServiceLauncher::LaunchRenderer(std::vector<char*>& argv) {
  OSAL_PROCESS_ID pid;
  OSAL_PROCESS_ID tid;

  if (strncmp(argv[0], kProcSelfExe, strlen(kProcSelfExe)) == 0)
    argv[0] = const_cast<char*>(chromium_path_);

  DPRINT(COMM, DEBUG_INFO, "Renderer will be launched: %s\n", argv[0]);

  if (!__OSAL_Create_Child_Process(argv, &pid, &tid)) {
    return false;
  }
  children_.push_back(pid);
  return true;
}
