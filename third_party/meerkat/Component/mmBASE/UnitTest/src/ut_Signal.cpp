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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "bDataType.h"
#include "bGlobDef.h"
#include "Debugger.h"
#include "bThread.h"
#include "posixAPI.h"

using namespace mmBase;

OSAL_Event_Handle g_Event;
OSAL_Mutex_Handle g_Mutex;

class Thread1 : public CbThread {
 public:
  Thread1();
  Thread1(const CHAR* name) : CbThread(name) {}
  virtual ~Thread1() {}
  void MainLoop(void* args) {
    while (m_bRun) {
      DPRINT(COMM, DEBUG_INFO, "[Thread 1] Wait Signal\n");
      while (true) {
        OSAL_Event_Status ret = __OSAL_Event_Wait(&g_Mutex, &g_Event, -1);
        if (ret == OSAL_EVENT_WAIT_TIMEOUT) {
          DPRINT(COMM, DEBUG_INFO, "[Thread 1] Wait Event Timeout\n");
        } else if (ret == OSAL_EVENT_WAIT_GETSIG) {
          DPRINT(COMM, DEBUG_INFO, "[Thread 1] Get Event\n");
          break;
        } else {
          DPRINT(COMM, DEBUG_INFO, "[Thread 1] Event Error\n");
        }
      }
    }
  }
};

class Thread2 : public CbThread {
 public:
  Thread2() {}
  Thread2(const CHAR* name) : CbThread(name) {}
  virtual ~Thread2() {}
  void MainLoop(void* args) {
    while (m_bRun) {
      // DPRINT(COMM,DEBUG_INFO, "Thread 2 Running\n ");
      __OSAL_Sleep(3000);
      DPRINT(COMM, DEBUG_INFO, "[Thread 2] Send Signal\n ");
      __OSAL_Event_Send(&g_Event);
    }
  }
};

int main(void) {
  InitDebugInfo(TRUE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  g_Mutex = __OSAL_Mutex_Create();
  g_Event = __OSAL_Event_Create();

  Thread1 th1("thread1");
  Thread2 th2("thread2");

  th1.StartMainLoop(NULL);
  th2.StartMainLoop(NULL);
  while (true) {
    char ch = getchar();
    if (ch == 'q')
      break;
  }
  th1.StopMainLoop();
  th2.StopMainLoop();

  return 0;
}
