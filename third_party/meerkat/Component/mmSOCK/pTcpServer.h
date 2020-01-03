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

#ifndef __INCLUDE_NETCOMP_TCPSERVERSOCK_H__
#define __INCLUDE_NETCOMP_TCPSERVERSOCK_H__

#include "bList.h"
#include "bSocket.h"
#include "bTask.h"

namespace mmProto {

typedef void (*pNetDataCBFunc)(VOID* pListner,
                               OSAL_Socket_Handle iEventSock,
                               const CHAR* pszsource_address,
                               long source_port,
                               CHAR* pData,
                               INT32 iLen);
class CpAcceptSock : public mmBase::CbTask, public mmBase::CbSocket {
 public:
  struct connection_info {
    OSAL_Socket_Handle clientSock;
    char clientAddr[16];
    CpAcceptSock* pConnectionHandle;
    bool authorized;
  };

 public:
  CpAcceptSock(const char* pszQname);
  CpAcceptSock(const char* pszQname, SSL* ssl);
  virtual ~CpAcceptSock();

  VOID Activate(OSAL_Socket_Handle iSock,
                VOID* pListenerPtr,
                pNetDataCBFunc lpDataCallback,
                INT32 nReadBytePerOnce = -1,
                INT32 lNetworkEvent = FD_READ);
  VOID DeActivate();
  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszsource_address,
                         long source_port,
                         CHAR* pData,
                         INT32 iLen);
  virtual VOID OnClose(OSAL_Socket_Handle iSock);
  virtual BOOL OnAccept(OSAL_Socket_Handle iSock, CHAR* szConnectorAddr) {
    return true;
  }
  OSAL_Socket_Handle GetSockHandle() { return m_hSock; }

 private:
  void Begin(void) {}
  void MainLoop(void* args);
  void Endup(void) {}

  pNetDataCBFunc m_lpDataCallback;
  VOID* m_pListenerPtr;

  INT32 m_nReadBytePerOnce;
  INT32 m_hListenerMonitor;
  OSAL_Event_Handle m_hTerminateEvent;
  OSAL_Mutex_Handle m_hTerminateMutex;
  OSAL_Socket_EventObj m_hListenerEvent;
};

/**
 * @class        CpTcpServer
 * @brief        tcp server socket base class header
 * @author      Namgung Eun
 * @date         2008/06/30
 */

class CpTcpServer : public mmBase::CbTask, public mmBase::CbSocket {
 public:
  CpTcpServer();
  CpTcpServer(const char* msgq_name);
  virtual ~CpTcpServer();
  BOOL Create();
  BOOL Open(INT32 iPort = DEFAULT_SOCK_PORT);
  BOOL Start(INT32 iBackLog = SOMAXCONN,
             INT32 nReadBytePerOnce = -1,
             INT32 lNetworkEvent = FD_ACCEPT |
                                   FD_CLOSE) /*FD_READ|FD_WRITE|FD_OOB |
                                                FD_ACCEPT | FD_CONNECT |
                                                FD_CLOSE*/
      ;
  BOOL Stop() { return Stop(m_hSock); }
  BOOL Stop(OSAL_Socket_Handle iSock);

  BOOL Close();

  virtual VOID EventNotify(OSAL_Socket_Handle iEventSock,
                           CbSocket::SOCKET_NOTIFYTYPE type) = 0;
  virtual VOID DataRecv(OSAL_Socket_Handle iEventSock,
                        const CHAR* pszsource_address,
                        long source_port,
                        CHAR* pData,
                        INT32 iLen) = 0;

  virtual INT32 DataSend(const CHAR* pAddress, CHAR* pData, INT32 iLen);
  virtual INT32 DataSend(OSAL_Socket_Handle iSock, CHAR* pData, INT32 iLen);

  CHAR* Address(OSAL_Socket_Handle iSock);

  bool use_ssl() const { return use_ssl_; }
  void set_use_ssl(bool use_ssl) { use_ssl_ = use_ssl; }

 protected:
  virtual BOOL OnAccept(OSAL_Socket_Handle iSock, CHAR* szConnectorAddr);
  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszsource_address,
                         long source_port,
                         CHAR* pData,
                         INT32 iLen) {}
  virtual VOID OnClose(OSAL_Socket_Handle iSock) {}

  static void ReceiveCallback(VOID* pListener,
                              OSAL_Socket_Handle iEventSock,
                              const CHAR* pszsource_address,
                              long source_port,
                              CHAR* pData,
                              INT32 iLen);

  CpAcceptSock::connection_info* GetConnectionHandle(OSAL_Socket_Handle iSock);
  CpAcceptSock::connection_info* GetConnectionHandle(const CHAR* pAddress);
  BOOL DelConnectionHandle(OSAL_Socket_Handle iSock);
  BOOL DelConnectionHandle(const CHAR* pAddress);

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

  mmBase::CbList<CpAcceptSock::connection_info> m_ConnList;

 private:
  bool use_ssl_;
  SSL_CTX* ssl_ctx_;
};
}  // namespace mmProto
#endif

/***********************************************************************************
----------------------------------------------------------
Usage :
----------------------------------------------------------

#include "../NetworkClass/CpTcpServer .h"

class CCustomTcpServer : public CpTcpServer
{
public:
        CCustomTcpServer(): CpTcpServer(){}
        CCustomTcpServer(const CHAR* msgqname):CpTcpServer(msgqname){}
        virtual ~CCustomTcpServer(){}

        BOOL StartServer(int port, int readperonce=-1){
                printf("start server with [%d] port\n",port);
                CpTcpServer::Create();
                CpTcpServer::Open(port);
                CpTcpServer::Start(readperonce);
                return TRUE;
        }

        BOOL StopServer(){
                return TRUE;
        }

        VOID DataRecv(OSAL_Socket_Handle iEventSock, CHAR* pData, INT32 iLen){

                RAW_PRINT("Receive- from:[%s]
msg:[%s]\n",Address(iEventSock),pData);
        }

        VOID EventNotify(OSAL_Socket_Handle eventSock,
CbSocket::SOCKET_NOTIFYTYPE type){
                RAW_PRINT("Get Notify- form:sock[%d] event[%d]\n",eventSock,
type);
        }

private:

protected:

};

int main(int argc, char** argv)
{
        SetModuleDebugFlag(MODULE_ALL,TRUE);
        StartMessageMonitor();
        if(argc<2)
        {
                RAW_PRINT("Too Few Argument!!\n");
                RAW_PRINT("Type : [TcpClientTest port]!!\n");
                return 0;
        }

        CCustomTcpServer* p=new CCustomTcpServer;
        p->StartServer(atoi(argv[1]));

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
