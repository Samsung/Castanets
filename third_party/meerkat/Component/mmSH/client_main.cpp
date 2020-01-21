/*
 * Copyright 2019 Samsung Electronics Co., Ltd
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

#include <string>

#include "Debugger.h"
#include "client_runner.h"

#if defined(WIN32)
#include "spawn_controller.h"
#endif

static std::string GetToken() {
  return "client-token-sample";
}

static bool VerifyToken(const char* token) {
  return true;
}

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
int real_main(HANDLE ev_term, int argc, char** argv) {
#else
int real_main(int argc, char** argv) {
#endif
  ClientRunner::ClientRunnerParams params;
  if (!ClientRunner::BuildParams("client.ini", params) &&
      !ClientRunner::BuildParams("/usr/bin/client.ini", params) &&
      !ClientRunner::BuildParams(argc, argv, params))
    return -1;

  params.get_token = &GetToken;
  params.verify_token = &VerifyToken;

  auto client_runner = new ClientRunner(params);
  int exit_code = client_runner->Initialize();
  if (exit_code > 0)
    return exit_code;

#if defined(WIN32) && defined(RUN_AS_SERVICE)
  exit_code = client_runner->Run(ev_term);
#else
  exit_code = client_runner->Run();
#endif

  return exit_code;
}

int main(int argc, char** argv) {
#if defined(WIN32) && defined(RUN_AS_SERVICE)
  CSpawnController::getInstance().ServiceRegister(real_main);
  return 0;
#else
  return real_main(argc, argv);
#endif
}
