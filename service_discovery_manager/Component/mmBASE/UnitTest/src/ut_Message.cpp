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

using namespace mmBase;

static pthread_t g_hThread1, g_hThread2, g_hThread3;
static int g_running_thread2 = 1;
static int g_running_thread3 = 1;

static void* thread1(void* args) {
  int i = 0;
  MSG_PACKET packet_s;
  char buff[1024];
  CbMessage* pMsgth2 = GetThreadMsgInterface("thread2");
  CbMessage* pMsgth3 = GetThreadMsgInterface("thread3");
  if (!pMsgth2) {
    DPRINT(COMM, DEBUG_INFO, "No Task2Msg\n");
    return NULL;
  }

  if (!pMsgth3) {
    DPRINT(COMM, DEBUG_INFO, "No Task3Msg\n");
    return NULL;
  }

  while (1) {
    sprintf(buff, "Thread1-Message%d", i);
    packet_s.id = 0x10;
    packet_s.wParam = 0x0;
    packet_s.lParam = 0x0;
    packet_s.msgdata = (unsigned char*)buff;
    packet_s.len = strlen(buff);
    DPRINT(GLOB, DEBUG_FATAL, "Thread1--Send Msg/ cmd=[%d] data=[%s]\n",
           packet_s.id, (char*)packet_s.msgdata);
    pMsgth2->Send(&packet_s, MSG_UNICAST);
    pMsgth3->Send(&packet_s, MSG_UNICAST);
    i++;

    __OSAL_Sleep(1000);
  }

  DPRINT(COMM, DEBUG_INFO,
         "Enter while loop to wait for end point of receiving\n");
  while (g_running_thread2 || g_hThread3)
    sched_yield();
  DPRINT(COMM, DEBUG_INFO, "End of while loop\n");
  return NULL;
}

static void* thread2(void* args) {
  MSG_PACKET packet_r;
  CbMessage* pmsg = (CbMessage*)args;
  CbMessage* pMsgth1 = GetThreadMsgInterface("thread1");
  if (!pMsgth1) {
    DPRINT(COMM, DEBUG_INFO, "No Task1Msg\n");
    return NULL;
  }
  while (1) {
    pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER);
    DPRINT(COMM, DEBUG_ERROR, "Thread2--Recv Msg/ cmd=[%d] data=[%s]\n",
           packet_r.id, (char*)packet_r.msgdata);
  }
  g_running_thread2 = 0;
  printf("thread2 End\n");
  return NULL;
}

static void* thread3(void* args) {
  MSG_PACKET packet_r;
  CbMessage* pmsg = (CbMessage*)args;
  CbMessage* pMsgth1 = GetThreadMsgInterface("thread1");
  if (!pMsgth1) {
    DPRINT(COMM, DEBUG_INFO, "No Task1Msg\n");
    return NULL;
  }
  while (1) {
    pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER);
    DPRINT(CONN, DEBUG_WARN, "Thread3--Recv Msg/ cmd=[%d] data=[%s]\n",
           packet_r.id, (char*)packet_r.msgdata);
  }
  g_running_thread3 = 0;
  DPRINT(COMM, DEBUG_INFO, "thread3 End\n");
  return NULL;
}

int main(void) {
  int rc;
  InitDebugInfo(TRUE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_DETAIL);

  CbMessage msg1("thread1");
  CbMessage msg2("thread2");
  CbMessage msg3("thread3");

  rc = pthread_create(&g_hThread2, NULL, thread2, (void*)&msg2);
  if (rc) {
    DPRINT(COMM, DEBUG_INFO, "pthread_create2 failed\n");
    return -1;
  }

  rc = pthread_create(&g_hThread3, NULL, thread3, (void*)&msg3);
  if (rc) {
    DPRINT(COMM, DEBUG_INFO, "pthread_create3 failed\n");
    return -1;
  }

  rc = pthread_create(&g_hThread1, NULL, thread1, (void*)&msg1);
  if (rc) {
    DPRINT(COMM, DEBUG_INFO, "pthread_create1 failed\n");
    return -1;
  }

  if (pthread_join(g_hThread1, NULL))
    perror("pthread_join failed");
  if (pthread_join(g_hThread2, NULL))
    perror("pthread_join failed");
  if (pthread_join(g_hThread3, NULL))
    perror("pthread_join failed");
  return 0;
}
