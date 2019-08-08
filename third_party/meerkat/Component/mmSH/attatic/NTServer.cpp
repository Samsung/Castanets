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

#include "NetworkService.h"
#include "osal.h"

#define VTUN_DEV_LEN 20

#define TUN_DEFAULT_PORT 5000
#define NET_READ_ONCED 10240

int main(int argc, char** argv) {
  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  // SetDebugLevel(DEBUG_FATAL);
  // SetDebugLevel(DEBUG_INFO);
  SetDebugLevel(DEBUG_LEVEL_MAX);
  SetDebugFormat(DEBUG_NORMAL);

  char* server_ip = NULL;
  unsigned short tun_port = TUN_DEFAULT_PORT;
  int read_once = NET_READ_ONCED;
  char* bucket[100];
  int cntBucket;

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
    }
  }

  if (server_ip == NULL) {
    DPRINT(COMM, DEBUG_ERROR, "%s argument is not set\n", argv[0]);
    DPRINT(COMM, DEBUG_ERROR, "Usage:\n");
    DPRINT(COMM, DEBUG_ERROR,
           "%s -server_addr [server ip] -stun_port [stun port] -read_once "
           "[sock read byte per once]\n");
    return 0;
  }

  RAW_PRINT("\t******************************************\n");
  RAW_PRINT("\t*     Start up Tiny STUN/TURN Server     *\n");
  RAW_PRINT("\t******************************************\n");

  CNetworkService* pService =
      new CNetworkService("netservice", server_ip, tun_port);
  pService->StartServer(tun_port, read_once);

  while (true) {
    DPRINT(COMM, DEBUG_INFO, "Server Debugging Menu\n");
    DPRINT(COMM, DEBUG_INFO, "table: Dump Route Mapping Table\n");
    DPRINT(COMM, DEBUG_INFO, "mtable: Dump Route Mapping Table\n");
    DPRINT(COMM, DEBUG_INFO, "relay: Dump Relay Channel Table\n");
    DPRINT(COMM, DEBUG_INFO, "quit: quit server\n");

    char input[32];
    memset(input, 0, 32);
    ignore_result(scanf("%s", input));

    if (!strcmp(input, "table")) {
      pService->DUMP_TABLE();
    } else if (!strcmp(input, "mtable")) {
      cntBucket = pService->MEMDUMP_TABLE(bucket);
      for (int i = 0; i < cntBucket; i++) {
        DPRINT(COMM, DEBUG_INFO, "Table no:%d, src:%s, mapped:%s\n",
               i, bucket[i * 4], bucket[i * 4 + 1]);
      }
    } else if (!strcmp(input, "relay")) {
      pService->DUMP_CHANNEL();
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

  pService->StopServer();
  SAFE_DELETE(pService);

  return 0;
}
