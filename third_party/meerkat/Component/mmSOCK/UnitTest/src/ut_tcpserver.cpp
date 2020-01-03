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

#ifdef WIN32

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif

#include "pTcpServer.h"
#include "string_util.h"

using namespace mmBase;
using namespace mmProto;

FILE* g_logFile;

class CCustomTcpServer : public CpTcpServer {
 public:
  CCustomTcpServer() : CpTcpServer() {}
  CCustomTcpServer(const CHAR* msgqname) : CpTcpServer(msgqname) {
    mmBase::strlcpy(name, msgqname, sizeof(name));
  }
  virtual ~CCustomTcpServer() {}

  BOOL StartServer(int port, int readperonce = -1) {
    printf("start server with [%d] port\n", port);
    CpTcpServer::Create();
    CpTcpServer::Open(port);
    CpTcpServer::Start(readperonce);
    m_count = 0;
    return TRUE;
  }

  BOOL StopServer() { return TRUE; }

  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen) {
    RAW_PRINT("Receive- from:[%d-%s] msg:[%s]\n", iEventSock,
              Address(iEventSock), pData);
  }

  VOID EventNotify(OSAL_Socket_Handle eventSock,
                   CbSocket::SOCKET_NOTIFYTYPE type) {
    RAW_PRINT("Get Notify- form:sock[%d] event[%d]\n", eventSock, type);
  }

 private:
  char name[64];
  int m_count;

 protected:
};

#ifdef WIN32
int ut_base_comp_tcpserver_test(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif
  if (argc < 2) {
    printf("usage : %s port\n", argv[0]);
    return 0;
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CCustomTcpServer* p = new CCustomTcpServer("magic");
  p->StartServer(atoi(argv[1]));

  while (true) {
    RAW_PRINT("Menu -- Quit:q Send:s\n");
    CHAR ch = getchar();
    getchar();
    if (ch == 'q') {
      RAW_PRINT("Quit Program\n");
      break;
    } else if (ch == 's') {
      CHAR str[256], ip[16];
      RAW_PRINT("Enter Client IP\n");
      scanf("%s", ip);
      // gets(ip);
      RAW_PRINT("Enter message\n");
      scanf("%s", str);
      // gets(str);
      p->DataSend(ip, str, strlen(str) + 1);
    }
  }
  p->Close();
  fclose(g_logFile);
  return 0;
}
