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

#ifndef __INCLUDE_RMT_SERVER_H__
#define __INCLUDE_RMT_SERVER_H__

//#include "bTask.h"
#include "pUdpServer.h"

typedef void (*pfReceiver)(int type, char* addr, int port, int len, void* data);

class CRmtServer : public mmProto::CpUdpServer {
 public:
  CRmtServer(const CHAR* msgqname) : CpUdpServer(msgqname) {}
  virtual ~CRmtServer() {}

  BOOL RemoteServerStart(pfReceiver receiver, int port, int readperonce = 1024);
  BOOL RemoteServerStop();
  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen);
  VOID EventNotify(OSAL_Socket_Handle eventSock,
                   CbSocket::SOCKET_NOTIFYTYPE type);

 private:
  pfReceiver m_pReceiver;

 protected:
};

#endif  //__INCLUDE_RMT_SERVER_H__
