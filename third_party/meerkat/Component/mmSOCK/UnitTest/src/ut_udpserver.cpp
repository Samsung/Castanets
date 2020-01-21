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

#include "pUdpServer.h"

#include "string_util.h"

using namespace mmBase;
using namespace mmProto;

class CCustomUdpServer : public CpUdpServer {
 public:
  CCustomUdpServer() : CpUdpServer() {}
  CCustomUdpServer(const CHAR* msgqname) : CpUdpServer(msgqname) {
    mmBase::strlcpy(name, msgqname, sizeof(name));
  }
  virtual ~CCustomUdpServer() {}

  BOOL StartServer(int port, int readperonce = -1) {
    printf("start server with [%d] port\n", port);
    CpUdpServer::Create();
    CpUdpServer::Open(port);
    CpUdpServer::Start(readperonce);
    m_count = 0;
    return TRUE;
  }

  BOOL StopServer() { return TRUE; }

  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen) {
    RAW_PRINT("Receive- from:[%s:%ld] msg:[%s]\n", pszsource_addr, source_port,
              pData);
    char eco_message[256] = {
        '\0',
    };
    sprintf(eco_message, "eco -- %s", pData);
    CpUdpServer::DataSend(pszsource_addr, eco_message, strlen(eco_message),
                          source_port);
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
int ut_base_comp_udpserver_test(int argc, char** argv) {
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

  CCustomUdpServer* p = new CCustomUdpServer("magic");
  p->StartServer(atoi(argv[1]));

  while (true) {
    RAW_PRINT("Menu -- Quit:q\n");
    CHAR ch = getchar();
    getchar();
    if (ch == 'q') {
      RAW_PRINT("Quit Program\n");
      break;
    }
  }
  p->Close();
  return 0;
}
