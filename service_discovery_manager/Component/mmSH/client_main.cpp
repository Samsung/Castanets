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

#include "bINIParser.h"
#include "client_runner.h"
#include "Debugger.h"

#if defined(WIN32)
#include "spawn_controller.h"
#endif

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
int real_main(HANDLE ev_term, int argc, char** argv) {
#else
int real_main(int argc, char** argv) {
#endif
  ClientRunner::ClientRunnerParams params;
  mmBase::CbINIParser settings;

  int ret = settings.Parse("server.ini");
  if (ret == -1)
    ret = settings.Parse("/usr/bin/server.ini");

  if (ret == 0) {
    params.multicast_addr = settings.GetAsString("multicast", "address", "");
    params.multicast_port = settings.GetAsInteger("multicast", "port", -1);
    params.presence_addr = settings.GetAsString("presence", "address", "");
    params.presence_port = settings.GetAsInteger("presence", "port", -1);
    params.with_presence = params.presence_addr.length() > 0 &&
                           params.presence_port > 0;
    params.is_daemon = settings.GetAsBoolean("run", "run-as-damon", false);
  } else {
    RAW_PRINT("ini parse error(%d)\n", ret);
    if (argc < 3) {
      RAW_PRINT("Too Few Argument!!\n");
      RAW_PRINT("usage : %s mc_addr mc_port <presence> <pr_addr> <pr_port> "
                "<daemon>\n", argv[0]);
      RAW_PRINT("comment: mc(multicast),\n");
      RAW_PRINT("         presence (default is 0. This need to come with "
                "pr_addr and pr_port once you use it)\n");
      RAW_PRINT("         daemon (default is 0. You can use it if you want\n");
      return 0;
    }
    params.multicast_addr = std::string(argv[1]);
    params.multicast_port = atoi(argv[2]);
    params.is_daemon = (argc == 4 && (strncmp(argv[5], "daemon", 6) == 0)) ||
                       (argc == 7 && (strncmp(argv[8], "daemon", 6) == 0));
    params.with_presence = (argc >= 6 && (strncmp(argv[3], "presence", 8) == 0));
    if (params.with_presence) {
      params.presence_addr = std::string(argv[4]);
      params.presence_port = atoi(argv[5]);
    }
  }

  auto client_runner = new ClientRunner(params);
  int exit_code = client_runner->Initialize();
  if (exit_code > 0)
    return exit_code;

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
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
