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

#include "Debugger.h"
#include "bDataType.h"

#include "NetworkService.h"
#include "osal.h"

#define VTUN_DEV_LEN 20

int main(int argc, char** argv) {
  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  if (argc < 3) {
    RAW_PRINT("%s Launched with too few argument\n", argv[0]);
    RAW_PRINT("%s Usage:\n", argv[0]);
    RAW_PRINT("%s [server address] [stun port]:\n", argv[0]);
    RAW_PRINT("eg. %s 192.168.0.100 5000\n", argv[0]);
    return 0;
  }

  RAW_PRINT("\t******************************************\n");
  RAW_PRINT("\t*     Start up Tiny STUN/TURN Server     *\n");
  RAW_PRINT("\t******************************************\n");

  CNetworkService* pService =
      new CNetworkService("netservice", argv[1], (unsigned short)atoi(argv[2]));
  pService->StartServer(5000, -1);

  while (true) {
    DPRINT(COMM, DEBUG_INFO, "Server Debugging Menu\n");
    DPRINT(COMM, DEBUG_INFO, "table: Dump Route Mapping Table\n");
    DPRINT(COMM, DEBUG_INFO, "relay: Dump Relay Channel Table\n");
    DPRINT(COMM, DEBUG_INFO, "quit: quit server\n");

    char input[32];
    memset(input, 0, 32);
    scanf("%s", input);

    if (!strcmp(input, "table")) {
      pService->DUMP_TABLE();
    } else if (!strcmp(input, "relay")) {
      pService->DUMP_CHANNEL();
    } else {
      DPRINT(COMM, DEBUG_INFO, "unknown request [%s]\n", input);
    }

    __OSAL_Sleep(1000);
  }

  pService->StopServer();
  SAFE_DELETE(pService);

  return 0;
}
