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

#ifndef __INCLUDE_MESSAGE_HANDLER_H__
#define __INCLUDE_MESSAGE_HANDLER_H__

#include "Debugger.h"
#include "bDataType.h"
#include "bGlobDef.h"
//#include "bDataType.h"

#define MQWTIME_WAIT_SLICE 10  // msec
#define MQWTIME_WAIT_FOREVER -1
#define MQWTIME_WAIT_NO 0

#define MQ_INVALIDHANDLE 0
#define MQ_MAXNAMELENGTH 64

#define MESSAGE_RECEIVE_TIMEOUT -1
#define MESSAGE_RECEIVE_ERROR -2

namespace mmBase {
class CbMessage {
  //// class method declaration ////
  //// public method

 public:
  CbMessage();
  CbMessage(const char* szname);
  virtual ~CbMessage();

  int Send(PMSG_PACKET pPacket, E_MSG_TYPE e_type);
  int Send(int id,
           int wParam,
           int lParam,
           int len = 0,
           void* msgData = NULL,
           E_MSG_TYPE type = MSG_UNICAST);

  int Recv(PMSG_PACKET pPacket, int i_msec = 0);

  // protected method
 protected:
  int CreateMsgQueue(const char* szname);
  int DestroyMsgQueue(void);
  BOOL UnRegisterAllMonitor();

  // private member
 private:
  char m_szMQname[MQ_MAXNAMELENGTH];
  PMSGQ_HEAD m_iMQhandle;

 protected:
};

typedef CbMessage* pMsgHandle;
pMsgHandle GetThreadMsgInterface(const char* name);
}  // namespace mmBase

#endif  //__INCLUDE_MESSAGE_HANDLER_H__

/***********************************************************************************
----------------------------------------------------------
Usage :
----------------------------------------------------------
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "CiDataType.h"
#include "CiGlobDef.h"
#include "CiMsgBase.h"

static pthread_t g_hThread1, g_hThread2,g_hThread3;
static int g_running_thread2=1;
static int g_running_thread3=1;

static void* thread1(void *args)
{
        int i=0;
        MSG_PACKET packet_s;
        MSG_PACKET packet_r;
        char buff[1024];
        CiMsgBase *pmsg=(CiMsgBase *)args;
        CiMsgBase *pMsgth2 = GetThreadMsgInterface("thread2");
        CiMsgBase *pMsgth3 = GetThreadMsgInterface("thread3");
        if(!pMsgth2)
        {
                printf("No Task2Msg\n");
                return NULL;
        }

        if(!pMsgth3)
        {
                printf("No Task3Msg\n");
                return NULL;
        }

        while(1)
        {
                sprintf(buff, "Thread1-Message%d", i);
                packet_s.id=0x10;
                packet_s.wParam=0x0;
                packet_s.lParam=0x0;
                packet_s.msgdata=(unsigned char*)buff;
                packet_s.len=strlen(buff);
                pMsgth2->Send(&packet_s,MSG_UNICAST);
                pMsgth3->Send(&packet_s,MSG_UNICAST);
                i++;
                printf("Thread1--Send Msg/ cmd=[%d] data=[%s]\n", packet_s.id,
(char*)packet_s.msgdata);

                if(pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER)>0)
                        printf("Thread1--Recv Echo/ cmd=[%d] data=[%s]\n",
packet_r.id,(char*)packet_r.msgdata);
                if(pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER)>0)
                        printf("Thread1--Recv Echo/ cmd=[%d] data=[%s]\n",
packet_r.id,(char*)packet_r.msgdata);

                usleep(500000);
        }

        printf("Enter while loop to wait for end point of receiving\n");
        while(g_running_thread2||g_hThread3) sched_yield();
        printf("End of while loop\n");
        return NULL;
}

static void* thread2(void *args)
{
        char buff[1024];
        MSG_PACKET packet_r;
        MSG_PACKET packet_s;
        CiMsgBase *pmsg=(CiMsgBase *)args;
        CiMsgBase *pMsgth1 = GetThreadMsgInterface("thread1");
        if(!pMsgth1)
        {
                printf("No Task1Msg\n");
                return NULL;
        }
        while(1)
        {
                pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER);
                printf("Thread2--Recv Msg/ cmd=[%d] data=[%s]\n",
packet_r.id,(char*)packet_r.msgdata);
                usleep(5000);
                sprintf(buff, "/Echo thread2/");
                packet_s.id=0x11;
                packet_s.wParam=0x0;
                packet_s.lParam=0x0;
                packet_s.msgdata=(unsigned char*)buff;
                packet_s.len=strlen(buff);

                pMsgth1->Send(&packet_s,MSG_UNICAST);
        }
        g_running_thread2 = 0;
        printf("thread2 End\n");
        return NULL;
}

static void* thread3(void *args)
{
        char buff[1024];
        MSG_PACKET packet_r;
        MSG_PACKET packet_s;
        CiMsgBase *pmsg=(CiMsgBase *)args;
        CiMsgBase *pMsgth1 = GetThreadMsgInterface("thread1");
        if(!pMsgth1)
        {
                printf("No Task1Msg\n");
                return NULL;
        }
        while(1)
        {
                pmsg->Recv(&packet_r, MQWTIME_WAIT_FOREVER);
                printf("Thread3--Recv Msg/ cmd=[%d] data=[%s]\n",
packet_r.id,(char*)packet_r.msgdata);
                usleep(5000);
                sprintf(buff, "/Echo thread3/");
                packet_s.id=0x11;
                packet_s.wParam=0x0;
                packet_s.lParam=0x0;
                packet_s.msgdata=(unsigned char*)buff;
                packet_s.len=strlen(buff);
                pMsgth1->Send(&packet_s,MSG_UNICAST);
        }
        g_running_thread3 = 0;
        printf("thread3 End\n");
        return NULL;
}

int main(void)
{
        int rc;
        CiMsgBase msg2("thread2");
        CiMsgBase msg3("thread3");
        CiMsgBase msg1("thread1");

        rc = pthread_create( &g_hThread2, NULL, thread2, (void*)&msg2 );
        if(rc)
        {
                printf("pthread_create2 failed\n");
                return -1;
        }
        rc = pthread_create( &g_hThread3, NULL, thread3, (void*)&msg3 );
        if(rc)
        {
                printf("pthread_create3 failed\n");
                return -1;
        }
        rc = pthread_create( &g_hThread1, NULL, thread1, (void*)&msg1 );
        if(rc)
        {
                printf("pthread_create1 failed\n");
                return -1;
        }
        if( pthread_join(g_hThread1, NULL) ) perror( "pthread_join failed" );
        if( pthread_join(g_hThread2, NULL) ) perror( "pthread_join failed" );
        if( pthread_join(g_hThread3, NULL) ) perror( "pthread_join failed" );
        return 0;
}
***********************************************************************************/
