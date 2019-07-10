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

#include "pUdpClient.h"

using namespace mmBase;
using namespace mmProto;

class CCustomUdpClient : public CpUdpClient {
 public:
  CCustomUdpClient() : CpUdpClient() {}
  CCustomUdpClient(const CHAR* msgqname) : CpUdpClient(msgqname) {}
  virtual ~CCustomUdpClient() {}

  BOOL StartClient(int readperonce = -1) {
    if (!CpUdpClient::Create())
      return false;
    if (!CpUdpClient::Open())
      return false;
    if (!CpUdpClient::Start(readperonce))
      return false;
    return TRUE;
  }

  BOOL StopClient() {
    CpUdpClient::Stop();
    CpUdpClient::Close();
    return TRUE;
  }

  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen) {
    RAW_PRINT("Receive:%s\n", pData);
  }

  VOID EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) {
    RAW_PRINT("Get Notify:%d\n", type);
  }

 private:
 protected:
};

#ifdef WIN32
int ut_base_comp_udpclient_test(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif
  if (argc < 3) {
    RAW_PRINT("Too Few Argument!!\n");
    RAW_PRINT("Type : [UdpClientTest ip port]!!\n");
    return 0;
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CCustomUdpClient* p = new CCustomUdpClient("client1");

  if (!p->StartClient()) {
    printf("cannot start client\n");
    return 0;
  }

  while (true) {
    RAW_PRINT("Menu -- Quit:q Send:s\n");
    CHAR ch = getchar();
    getchar();
    if (ch == 'q') {
      RAW_PRINT("Quit Program\n");
      break;
    } else if (ch == 's') {
      char str[] = "test message from client";
      p->DataSend(str, strlen(str) + 1, argv[1], atoi(argv[2]));
    }
  }

  p->Close();

  return 0;
}
