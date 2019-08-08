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

#include "bSocket.h"
#include "bThread.h"

using namespace mmBase;

#define ECO_SERVER_MSG "Eco Server Notify - ALIVE"
#define ECO_CLIENT_MSG "Eco Client Response - ACK"

class CEcoClient : public CbSocket, public CbThread {
 public:
  CEcoClient() {}
  virtual ~CEcoClient() {}

  BOOL CreateSocket(const CHAR* pszIPV4Addr, INT32 iPort) {
    if (CbSocket::Open(AF_INET, SOCK_STREAM, IPPROTO_TCP, ACT_TCP_CLIENT) !=
        OSAL_Socket_Success)
      return FALSE;
    int nCount = 100;
    while (nCount--) {
      if (CbSocket::Connect(pszIPV4Addr, iPort) == OSAL_Socket_Success) {
        DPRINT(COMM, DEBUG_INFO, "Connect Success\n");
        StartMainLoop(NULL);
        return TRUE;
      }
      __OSAL_Sleep(1000);
    }
    return FALSE;
  }
  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszAddress,
                         long source_port,
                         CHAR* pData,
                         INT32 iLen) {
    DPRINT(COMM, DEBUG_INFO, "Receive Data [%s] from Server\n", pData);
  }

  virtual VOID OnClose(OSAL_Socket_Handle iSock) {}

  void MainLoop(void* args) {
    while (CbThread::m_bRun) {
      CbSocket::Recv();
      CbSocket::Write((CHAR*)ECO_CLIENT_MSG, strlen(ECO_CLIENT_MSG));

      __OSAL_Sleep(1000);
    }
  }
};

class CEcoServer : public CbSocket, public CbThread {
 public:
  CEcoServer() {}
  virtual ~CEcoServer() {}

  BOOL CreateSocket(INT32 iPort) {
    if (CbSocket::Open(AF_INET, SOCK_STREAM, IPPROTO_TCP, ACT_TCP_SERVER) !=
        OSAL_Socket_Success) {
      DPRINT(COMM, DEBUG_ERROR, "Socket Create Error!!\n");
      return FALSE;
    }
    if (OSAL_Socket_Success != CbSocket::Bind(iPort)) {
      DPRINT(COMM, DEBUG_ERROR, "Socket Bind Error!!\n");
      return FALSE;
    }

    StartMainLoop(NULL);

    return FALSE;
  }

  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszAddress,
                         long source_port,
                         CHAR* pData,
                         INT32 iLen) {
    DPRINT(COMM, DEBUG_INFO, "Receive Data [%s] from <%s> Client\n", pData,
           pszAddress);
  }

  virtual VOID OnClose(OSAL_Socket_Handle iSock) {}

  void MainLoop(void* args) {
    DPRINT(COMM, DEBUG_ERROR, "MainLoop!!\n");
    OSAL_Socket_Handle AcceptSock;
    if (OSAL_Socket_Success != CbSocket::Listen(5)) {
      DPRINT(COMM, DEBUG_ERROR, "Socket Listen Error!!\n");
      return;
    }

    if (OSAL_Socket_Success != CbSocket::Accept(&AcceptSock)) {
      DPRINT(COMM, DEBUG_ERROR, "Socket Accept Error!!\n");
      return;
    }

    while (CbThread::m_bRun) {
      CbSocket::Write(AcceptSock, (CHAR*)ECO_SERVER_MSG,
                      strlen(ECO_SERVER_MSG));
      CbSocket::Recv(AcceptSock, -1);
      __OSAL_Sleep(1000);
    }
  }
};

#ifdef WIN32
int ut_base_comp_socket_test(int argc, char** argv) {
#else
int main(int argc, char* argv[]) {
#endif
  if (argc < 4) {
    printf("usage : %s type(s/c) ip port\n", argv[0]);
    return 0;
  }

  InitDebugInfo(TRUE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CEcoServer server;
  CEcoClient client;

  if (argv[1][0] == 's') {
    server.CreateSocket(atoi(argv[3]));
} else if (argv[1][0] == 'c') {
    client.CreateSocket(argv[2], atoi(argv[3]));
  }

  while (true) {
    char ch = getchar();
    if (ch == 'q')
      break;
  }
  return 0;
}
