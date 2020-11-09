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
#include "bDataType.h"
#include "bGlobDef.h"
#include "bTask.h"

using namespace mmBase;

#define SENDER_MQ_NAME "SENDER_MQ"
#define RECVER1_MQ_NAME "RECVER1_MQ"
#define RECVER2_MQ_NAME "RECVER2_MQ"

class Sender : public CbTask {
 public:
  Sender() {}
  Sender(const char* msgQ) : CbTask(msgQ) {}
  virtual ~Sender() {}

  void MainLoop(void* args) {
    int msg_count = 0;
    pReceiver1Q = GetThreadMsgInterface(RECVER1_MQ_NAME);
    pReceiver2Q = GetThreadMsgInterface(RECVER2_MQ_NAME);

    while (m_bRun) {
      DPRINT(COMM, DEBUG_INFO, "Sender--Send Message\n");
      if (Send(pReceiver1Q, 0x10, msg_count++, 0x1) < 0)
        DPRINT(COMM, DEBUG_ERROR, "Fail to Send Message\n");
      __OSAL_Sleep(100);
      if (Send(pReceiver1Q, 0x11, msg_count++, 0x1) < 0)
        DPRINT(COMM, DEBUG_ERROR, "Fail to Send Message\n");
      __OSAL_Sleep(100);
      if (Send(pReceiver2Q, 0x10, msg_count++, 0x2) < 0)
        DPRINT(COMM, DEBUG_ERROR, "Fail to Send Message\n");
      __OSAL_Sleep(100);

      __OSAL_Sleep(1000);
    }
  }

 private:
  CbMessage* pReceiver1Q;
  CbMessage* pReceiver2Q;
};

class Receiver1 : public CbTask {
 public:
  static void On_ThreadMessage1(int wParam,
                                int lParam,
                                void* pData,
                                void* pParent) {
    DPRINT(COMM, DEBUG_INFO,
           "Receive1::On_ThreadMessage1 Message :0x10 %d %d\n", wParam, lParam);
  }
  static void On_ThreadMessage2(int wParam,
                                int lParam,
                                void* pData,
                                void* pParent) {
    DPRINT(COMM, DEBUG_INFO,
           "Receive1::On_ThreadMessage2 Message :0x11 %d %d\n", wParam, lParam);
  }

 public:
  Receiver1() {}
  Receiver1(const char* msgQ) : CbTask(msgQ) {}
  virtual ~Receiver1() {}

  BOOL Create() {
    CbTask::Subscribe(0x10, Receiver1::On_ThreadMessage1);
    CbTask::Subscribe(0x11, Receiver1::On_ThreadMessage2);
    return CbTask::Create();
  }
  BOOL Destroy() {
    CbTask::UnSubscribe(0x10, Receiver1::On_ThreadMessage1);
    CbTask::UnSubscribe(0x11, Receiver1::On_ThreadMessage2);
    return CbTask::Destroy();
  }

 private:
};

class Receiver2 : public CbTask {
 public:
  static void On_ThreadMessage(int wParam,
                               int lParam,
                               void* pData,
                               void* pParent) {
    DPRINT(COMM, DEBUG_INFO, "Receive2::On_ThreadMessage Message :0x10 %d %d\n",
           wParam, lParam);
  }

 public:
  Receiver2() {}
  Receiver2(const char* msgQ) : CbTask(msgQ) {}
  virtual ~Receiver2() {}

  BOOL Create() {
    CbTask::Subscribe(0x10, Receiver2::On_ThreadMessage);
    return CbTask::Create();
  }
  BOOL Destroy() {
    CbTask::UnSubscribe(0x10, Receiver2::On_ThreadMessage);
    return CbTask::Destroy();
  }

 private:
};

#ifdef WIN32
int ut_base_comp_task_test(int argc, char** argv) {
#else
int main(int argc, char* argv[]) {
#endif
  InitDebugInfo(TRUE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  Sender s_th(SENDER_MQ_NAME);
  Receiver1 r_th1(RECVER1_MQ_NAME);
  Receiver2 r_th2(RECVER2_MQ_NAME);

  s_th.Create();
  r_th1.Create();
  r_th2.Create();

  while (true) {
    char ch = getchar();
    if (ch == 'q') {
      printf("qqqqq\n");
      break;
    } else if (ch == 'v') {
      printf("VVVV\n");
      r_th1.UnSubscribe(0x10, r_th1.On_ThreadMessage1);
    } else if (ch == 'k') {
      printf("kkkk\n");
      r_th1.UnSubscribe(0x11, r_th1.On_ThreadMessage2);
    } else if (ch == 'a') {
      printf("aaaa\n");
      r_th1.Subscribe(0x10, r_th1.On_ThreadMessage1);
    } else if (ch == 'b') {
      printf("bbbb\n");
      r_th1.Subscribe(0x11, r_th1.On_ThreadMessage2);
    }
  }

  s_th.Destroy();
  r_th1.Destroy();
  r_th2.Destroy();

  return 0;
}
