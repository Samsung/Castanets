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
#include "bGlobDef.h"
#include "NetTunProc.h"
#include "osal.h"

#define STUN_SERVER_IP "168.219.193.94"
#define TUN_DEFAULT_PORT 5000
#define NET_READ_ONCED 10240
#define STUN_EVW_UNIT 10000
#define BIND_REQ_PERIOD 1000
#define STUN_RETRY_COUNT 3

int main(int argc, char** argv) {
  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  // SetDebugLevel(DEBUG_FATAL);
  SetDebugLevel(DEBUG_INFO);
  // SetDebugLevel(DEBUG_LEVEL_MAX);
  SetDebugFormat(DEBUG_NORMAL);

  char* server_ip = NULL;
  unsigned short tun_port = TUN_DEFAULT_PORT;
  int read_once = NET_READ_ONCED;
  int time_unit = STUN_EVW_UNIT;
  int bind_period = BIND_REQ_PERIOD;
  int retry_count = STUN_RETRY_COUNT;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-server_addr"))  // STUN_SERVER_IP
    {
      if (argc > i) {
        server_ip = new char[16];
        memset(server_ip, 0, 16);
        strcpy(server_ip, argv[i + 1]);
      }
    } else if (!strcmp(argv[i], "-stun_port"))  // TUN_DEFAULT_PORT
    {
      if (argc > i)
        tun_port = (unsigned short)atoi(argv[i + 1]);
    } else if (!strcmp(argv[i], "-read_once"))  // NET_READ_ONCED
    {
      if (argc > i)
        read_once = atoi(argv[i + 1]);
    } else if (!strcmp(argv[i], "-time_unit"))  // STUN_EVW_UNIT
    {
      if (argc > i)
        time_unit = atoi(argv[i + 1]);
    } else if (!strcmp(argv[i], "-bind_period"))  // BIND_REQ_PERIOD
    {
      if (argc > i)
        bind_period = atoi(argv[i + 1]);
    } else if (!strcmp(argv[i], "-retry"))  // STUN_RETRY_COUNT
    {
      if (argc > i)
        retry_count = atoi(argv[i + 1]);
    }
  }

  if (server_ip == NULL) {
    DPRINT(COMM, DEBUG_ERROR, "%s argument is not set\n", argv[0]);
    DPRINT(COMM, DEBUG_ERROR, "Usage:\n");
    DPRINT(COMM, DEBUG_ERROR,
           "%s -server_addr [server ip] -stun_port [stun port] -read_once "
           "[sock read byte per once] -time_unit [unit time value(ms)] "
           "-bind_period [period of binding request(ms)] -retry [retry count "
           "per stun msg]\n");
    return 0;
  }

  RAW_PRINT("/t******************************************\n");
  RAW_PRINT("/t*     Start up Tiny STUN/TURN Client     *\n");
  RAW_PRINT("/t******************************************\n");

  CNetTunProc* pTunClientProcess =
      new CNetTunProc("tunprocess", server_ip, tun_port, read_once, time_unit,
                      bind_period, retry_count);
  pTunClientProcess->Create();

  while (true) {
    DPRINT(COMM, DEBUG_INFO, "Client Debugging Menu\n");
    DPRINT(COMM, DEBUG_INFO, "table: Dump Route Mapping Table\n");
    DPRINT(COMM, DEBUG_INFO, "relay: Dump Relay Channel Table\n");
    DPRINT(COMM, DEBUG_INFO, "quit: quit server\n");

    char input[32];
    memset(input, 0, 32);
    ignore_result(scanf("%s", input));

    if (!strcmp(input, "table")) {
      pTunClientProcess->DUMP_TABLE();
    } else if (!strcmp(input, "relay")) {
      pTunClientProcess->DUMP_CHANNEL();
    } else if (!strcmp(input, "quit")) {
      break;
    } else if (!strcmp(input, "debug")) {
      printf("1: info\n");
      printf("2: fatal\n");
      printf("3: max\n");
      memset(input, 0, 32);

      ignore_result(scanf("%s", input));
      if (!strcmp(input, "1")) {
        SetDebugLevel(DEBUG_INFO);
      } else if (!strcmp(input, "2")) {
        SetDebugLevel(DEBUG_FATAL);
      } else if (!strcmp(input, "3")) {
        SetDebugLevel(DEBUG_LEVEL_MAX);
      }
    } else {
      DPRINT(COMM, DEBUG_INFO, "unknown request [%s]\n", input);
    }

    __OSAL_Sleep(1000);
  }

  pTunClientProcess->Destroy();
  SAFE_DELETE(pTunClientProcess);
  SAFE_DELETE(server_ip);
  return 0;
}
