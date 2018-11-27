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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "bDataType.h"
#include "bGlobDef.h"
#include "Debugger.h"
#include "bMessage.h"
#include "Dispatcher.h"

using namespace mmBase;

static pthread_t g_hThread;
bool g_bRun;

static void* thread(void* args) {
  MSG_PACKET packet_s;
  char buff[1024];
  int i = 0;
  CbMessage* pMsgth1 = GetThreadMsgInterface("mq1");
  CbMessage* pMsgth2 = GetThreadMsgInterface("mq2");
  if (!pMsgth1) {
    DPRINT(COMM, DEBUG_INFO, "No Msg1\n");
    return NULL;
  }

  if (!pMsgth2) {
    DPRINT(COMM, DEBUG_INFO, "No Msg2\n");
    return NULL;
  }

  while (g_bRun) {
    packet_s.id = 1001;
    packet_s.wParam = 0x0;
    packet_s.lParam = 0x0;
    sprintf(buff, "send message to [msg1] %d\n", i);
    packet_s.msgdata = (unsigned char*)buff;
    packet_s.len = strlen(buff);
    pMsgth1->Send(&packet_s, MSG_UNICAST);

    packet_s.id = 1002;
    packet_s.wParam = 0x0;
    packet_s.lParam = 0x0;
    sprintf(buff, "send message to [msg2] %d\n", i);
    packet_s.msgdata = (unsigned char*)buff;
    packet_s.len = strlen(buff);
    pMsgth2->Send(&packet_s, MSG_UNICAST);
    i++;
    __OSAL_Sleep(1000);
  }

  return NULL;
}

static void OnEvent_mq1(int wParam, int lParam, void* pData, void* pParent) {
  DPRINT(CONN, DEBUG_INFO, "%s", (char*)pData);
}

static void OnEvent_mq2(int wParam, int lParam, void* pData, void* pParent) {
  DPRINT(CONN, DEBUG_INFO, "%s", (char*)pData);
}

int main(void) {
  int rc;
  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_DETAIL);

  CbMessage msg1("mq1");
  CbMessage msg2("mq2");
  CbMessage msg3("mq3");

  CSTI<CbDispatcher>::getInstancePtr()->Initialize();

  CSTI<CbDispatcher>::getInstancePtr()->Subscribe(1001, (void*)&msg1,
                                                  OnEvent_mq1);
  CSTI<CbDispatcher>::getInstancePtr()->Subscribe(1002, (void*)&msg2,
                                                  OnEvent_mq2);

  g_bRun = true;
  rc = pthread_create(&g_hThread, NULL, thread, (void*)&msg3);
  if (rc) {
    DPRINT(COMM, DEBUG_ERROR, "pthread_create failed\n");
    return -1;
  }

  while (true) {
    char ch = getchar();
    if (ch == 'q') {
      DPRINT(COMM, DEBUG_INFO, "Break\n");

      break;
    }
  }

  g_bRun = false;
  if (pthread_join(g_hThread, NULL))
    perror("pthread_join failed");

  CSTI<CbDispatcher>::getInstancePtr()->DeInitialize();
  CSTI<CbDispatcher>::releaseInstance();
  return 0;
}
