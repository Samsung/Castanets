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

//#include <pthread.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <sys/time.h>
//#include <unistd.h>
//#include "Debugger.h"
//#include "bDataType.h"
//#include "bGlobDef.h"

#include "osal.h"
#include "bMessage.h"

using namespace mmBase;


static OSAL_Thread_Handle hThread1, hThread2 , hThread3;
static int g_running1, g_running2, g_running3;

static void* thread1(void* args) {
  int i = 0;
  MSG_PACKET packet_s;
  char buff[1024];
  g_running1 = 1;
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

  while (g_running1) {
    sprintf(buff, "Thread1-Message%d", i);
    packet_s.id = 0x10;
    packet_s.wParam = 0x0;
    packet_s.lParam = 0x0;
    packet_s.msgdata = (unsigned char*)buff;
    packet_s.len = strlen(buff);
    DPRINT(GLOB, DEBUG_FATAL, "Thread1--Send Msg/ cmd=[%d] data=[%s]\n",
           packet_s.id, (char*)packet_s.msgdata);
	 __OSAL_Sleep(100);
    pMsgth2->Send(&packet_s, MSG_UNICAST);
	 __OSAL_Sleep(100);
    pMsgth3->Send(&packet_s, MSG_UNICAST);
    i++;
    __OSAL_Sleep(1000);
  }
/*
  DPRINT(COMM, DEBUG_INFO,
         "Enter while loop to wait for end point of receiving\n");
  while (g_running_thread2 || g_hThread3)
    sched_yield();
*/ 
 DPRINT(COMM, DEBUG_INFO, "End of while loop\n");
  
  return NULL;
}

static void* thread2(void* args) {
  MSG_PACKET packet_r;
  g_running2 = 1;
  CbMessage* pmsg = (CbMessage*)args;
  CbMessage* pMsgth1 = GetThreadMsgInterface("thread1");
  if (!pMsgth1) {
    DPRINT(COMM, DEBUG_INFO, "No Task1Msg\n");
    return NULL;
  }
  while (g_running2) {
    pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER);
    DPRINT(COMM, DEBUG_ERROR, "Thread2--Recv Msg/ cmd=[%d] data=[%s]\n",
           packet_r.id, (char*)packet_r.msgdata);
  }

  printf("thread2 End\n");
  return NULL;
}

static void* thread3(void* args) {
  MSG_PACKET packet_r;
  g_running3 = 1;
  CbMessage* pmsg = (CbMessage*)args;
  CbMessage* pMsgth1 = GetThreadMsgInterface("thread1");
  if (!pMsgth1) {
    DPRINT(COMM, DEBUG_INFO, "No Task1Msg\n");
    return NULL;
  }
  while (g_running3) {
    pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER);
    DPRINT(CONN, DEBUG_WARN, "Thread3--Recv Msg/ cmd=[%d] data=[%s]\n",
           packet_r.id, (char*)packet_r.msgdata);
  }

  DPRINT(COMM, DEBUG_INFO, "thread3 End\n");
  return NULL;
}

#ifdef WIN32
int ut_base_comp_message_test(int argc, char** argv) {
#else
int main(int argc, char* argv[]) {
#endif
  
  InitDebugInfo(TRUE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CbMessage msg1("thread1");
  CbMessage msg2("thread2");
  CbMessage msg3("thread3");

  hThread2=__OSAL_Create_Thread((void*)thread2, (void*)&msg2);
  if (hThread2 == NULL) {
	DPRINT(COMM, DEBUG_INFO, "create ((2)) create failed\n");
    return -1;
  }
  
  hThread3 =__OSAL_Create_Thread((void*)thread3, (void*)&msg3);
  if (hThread3 == NULL) {
    DPRINT(COMM, DEBUG_INFO, "create ((3)) create failed\n");
    return -1;
  }
  
  hThread1 = __OSAL_Create_Thread((void*)thread1, (void*)&msg1);
  if (hThread1 == NULL) {
    DPRINT(COMM, DEBUG_INFO, "create ((1)) create failed\n");
    return -1;
  }

  while (true) {
    char ch = getchar();
	if (ch == 'q') {
		g_running1 = g_running2 = g_running3 = 0;
		break;
	}
  }

  __OSAL_Join_Thread(hThread1, 100);
  __OSAL_Join_Thread(hThread2, 100);
  __OSAL_Join_Thread(hThread3, 100);

  return 0;
}
