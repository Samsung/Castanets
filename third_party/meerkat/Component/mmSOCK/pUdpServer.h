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

#ifndef __INCLUDE_COMMON_NETWORKCLASS_UDPSERVERSOCK_H__
#define __INCLUDE_COMMON_NETWORKCLASS_UDPSERVERSOCK_H__

#include "bSocket.h"
#include "bTask.h"

namespace mmProto {

class CpUdpServer : public mmBase::CbTask, public mmBase::CbSocket {
 public:
  CpUdpServer();
  CpUdpServer(const CHAR* msgqname);
  virtual ~CpUdpServer();
  BOOL Create();
  BOOL Open(INT32 iPort = DEFAULT_SOCK_PORT);
  BOOL Join(const CHAR* channel_addr);
  BOOL Start(INT32 nReadBytePerOnce = -1,
             INT32 lNetworkEvent = FD_READ |
                                   FD_CLOSE) /*FD_READ|FD_WRITE|FD_OOB |
                                                FD_ACCEPT | FD_CONNECT |
                                                FD_CLOSE*/
      ;
  BOOL Stop();
  BOOL Close();

  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszsource_addr,
                         long source_port,
                         CHAR* pData,
                         INT32 iLen);
  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         CHAR* pData,
                         INT32 iLen) {}
  virtual BOOL OnAccept(OSAL_Socket_Handle iSock, CHAR* szConnectorAddr) {
    return true;
  }
  virtual VOID OnClose(OSAL_Socket_Handle iSock) {}

  virtual VOID EventNotify(OSAL_Socket_Handle iEventSock,
                           CbSocket::SOCKET_NOTIFYTYPE type) = 0;
  virtual VOID DataRecv(OSAL_Socket_Handle iEventSock,
                        const CHAR* pszsource_addr,
                        long source_port,
                        CHAR* pData,
                        INT32 iLen) = 0;
  virtual INT32 DataSend(const CHAR* pAddress,
                         CHAR* pData,
                         INT32 iLen,
                         INT32 port);
  virtual INT32 DataSend(const CHAR* pAddress, CHAR* pData, INT32 iLen);

 private:
  void Begin(void) {}
  void MainLoop(void* args);
  void Endup(void) {}

 protected:
  OSAL_Event_Handle m_hTerminateEvent;
  OSAL_Mutex_Handle m_hTerminateMutex;
  OSAL_Socket_EventObj m_hListenerEvent;
  INT32 m_nReadBytePerOnce;
  INT32 m_hListenerMonitor;
};
}  // namespace mmProto
#endif

/***********************************************************************************
----------------------------------------------------------
Usage :
----------------------------------------------------------

#include "../NetworkClass/CpUdpServer.h"

class CCustomUdpServer : public CpUdpServer
{
  public:
    CustomUdpServer(): CpUdpServer(){}
    CustomUdpServer(const CHAR* msgqname):CpUdpServer(msgqname){}
    virtual ~CCustomUdpServer(){}

    BOOL StartServer(int port, int readperonce=-1){
      CpUdpServer::Create();
      CpUdpServer::Open(port);
      CpUdpServer::Start(readperonce);
      return TRUE;
    }

    BOOL StopServer(){
      return TRUE;
    }

    VOID DataRecv(OSAL_Socket_Handle iEventSock, const CHAR* pAddress, CHAR*
pData, INT32 iLen){
      RAW_PRINT("Receive - [form:%s] %s\n",pAddress,pData);
    }

    VOID EventNotify(OSAL_Socket_Handle iEventSock,
CiSocketBase::SOCKET_NOTIFYTYPE type){
      RAW_PRINT("Get Notify:%d\n",type);
    }

  private:

  protected:

};

int main(int argc, char** argv)
{
  SetModuleDebugFlag(MODULE_ALL,TRUE);
  if(argc<2)
  {
    RAW_PRINT("Too Few Argument!!\n");
    RAW_PRINT("Type : [TcpClientTest port]!!\n");
    return 0;
  }

  CCustomUdpServer* p=new CCustomUdpServer;
  p->StartServer(atoi(argv[1]),3);

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
       CHAR str[256], ip[16];
       RAW_PRINT("Enter Client IP\n");
       gets(ip);
       RAW_PRINT("Enter message\n");
       gets(str);
       p->DataSend(ip,str,strlen(str)+1);
     }
  }
  p->Close();
  return 0;
}

***********************************************************************************/
