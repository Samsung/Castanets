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

#ifndef __INCLUDE_DISCOVERY_SERVER_H__
#define __INCLUDE_DISCOVERY_SERVER_H__

#include "pUdpServer.h"

#define DEFAULT_SERVICE_PORT 10090
#define DEFAULT_MONITOR_PORT 10091

class CDiscoveryServer : public mmProto::CpUdpServer {
 public:
  CDiscoveryServer() : CpUdpServer() {
    m_service_port = DEFAULT_SERVICE_PORT;
    m_monitor_port = DEFAULT_MONITOR_PORT;
  }
  CDiscoveryServer(const CHAR* msgqname) : CpUdpServer(msgqname) {
    strcpy(name, msgqname);
    m_service_port = DEFAULT_SERVICE_PORT;
    m_monitor_port = DEFAULT_MONITOR_PORT;
  }

  virtual ~CDiscoveryServer() {}

  BOOL StartServer(const CHAR* channel_address, INT32 port, INT32 readperonce = -1);
  BOOL StopServer();

  VOID SetServiceParam(INT32 service_port, INT32 monitor_port);
  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen);
  VOID EventNotify(OSAL_Socket_Handle eventSock,
                   CbSocket::SOCKET_NOTIFYTYPE type);

 private:
  char name[64];
  int m_query_request_count;
  INT32 m_service_port;
  INT32 m_monitor_port;

 protected:
};

#endif
