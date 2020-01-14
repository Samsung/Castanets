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

#ifndef __INCLUDE_UDPCLIENTSOCK_H__
#define __INCLUDE_UDPCLIENTSOCK_H__

#include "bSocket.h"
#include "bTask.h"

namespace mmProto {
class CpUdpClient : public mmBase::CbTask, public mmBase::CbSocket {
 public:
  CpUdpClient();
  CpUdpClient(const CHAR* mgsqname);
  virtual ~CpUdpClient();

  BOOL Create();
  BOOL Open();
  BOOL SetTTL(UCHAR ttl);
  BOOL Start(
      INT32 nReadPerOnce = -1,
      INT32 lNetworkEvent =
          FD_READ) /*FD_READ|FD_WRITE|FD_OOB|FD_ACCEPT|FD_CONNECT|FD_CLOSE*/;
  BOOL Stop() { return Stop(m_hSock); }
  BOOL Stop(OSAL_Socket_Handle iSock);
  BOOL Close();
  INT32 DataSend(CHAR* pData, int iLen, const CHAR* szDestAddrIP, int port);
  virtual VOID DataRecv(OSAL_Socket_Handle iEventSock,
                        const CHAR* pszsource_addr,
                        long source_port,
                        CHAR* pData,
                        int iLen) = 0;
  virtual VOID EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) = 0;

 private:
  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszsource_addr,
                         long source_port,
                         CHAR* pData,
                         int iLen);
  virtual VOID OnClose(OSAL_Socket_Handle iSock);

  void Begin(void) {}
  void MainLoop(void* args);
  void Endup(void) {}

 public:
  INT32 m_nReadBytePerOnce;

 private:
  INT32 m_hListenerMonitor;

  OSAL_Event_Handle m_hTerminateEvent;
  OSAL_Mutex_Handle m_hTerminateMutex;

  OSAL_Socket_EventObj m_hListenerEvent;
};
}  // namespace mmProto
#endif

/***********************************************************************************
----------------------------------------------------------
Usage :
----------------------------------------------------------

#include "../NetworkClass/CpUdpClient.h"

class CCustomTcpClient : public CpUdpClient
{
public:
        CCustomTcpClient(): CpUdpClient(){}
        CCustomTcpClient(const CHAR* msgqname):CpUdpClient(msgqname){}
        virtual ~CCustomTcpClient(){}

        BOOL StartClient(const CHAR* pAddress, int port, int readperonce=-1){
                CpUdpClient::Create();
                CpUdpClient::Open(pAddress, port);
                CpUdpClient::Start(readperonce);
                return TRUE;
        }

        BOOL StopClient(){
                return TRUE;
        }

        VOID DataRecv(OSAL_Socket_Handle iEventSock, CHAR* pData, INT32 iLen){
                RAW_PRINT("Receive:%s\n",pData);
        }

        VOID EventNotify(CbSocket::SOCKET_NOTIFYTYPE type){
                RAW_PRINT("Get Notify:%d\n",type);
        }

private:

protected:

};

int main(int argc, char** argv)
{
        SetModuleDebugFlag(MODULE_ALL,TRUE);
        if(argc<3)
        {
                RAW_PRINT("Too Few Argument!!\n");
                RAW_PRINT("Type : [TcpClientTest ip port]!!\n");
                return 0;
        }

        CCustomTcpClient* p=new CCustomTcpClient;
        p->StartClient(argv[1],atoi(argv[2]),3);

        while(true)
        {
                RAW_PRINT("Menu -- Quit:q Send:s\n");
                CHAR ch=getchar();
                getchar();
                //fflush(stdin);
                if(ch=='q')
                {
                        RAW_PRINT("Quit Program\n");
                        break;
                }
                else if(ch=='s')
                {
                        CHAR *str="test message from client";
                        //RAW_PRINT("Enter message\n");
                        //scanf("%s",str);

                        p->DataSend(str,strlen(str)+1);
                }
        }
        p->Close();
        return 0;
}

***********************************************************************************/
