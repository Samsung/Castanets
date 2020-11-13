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

#include <string>

#include "pUdpServer.h"
#include "string_util.h"

#define DEFAULT_SERVICE_PORT 10090
#define DEFAULT_MONITOR_PORT 10091

using GetCapabilityFunc = std::string (*)();

class CDiscoveryServer : public mmProto::CpUdpServer {
 public:
  CDiscoveryServer();
  CDiscoveryServer(const CHAR* msgqname);

  virtual ~CDiscoveryServer() {}

  BOOL StartServer(const CHAR* channel_address,
                   INT32 port,
                   INT32 readperonce = -1);
  BOOL StopServer();

  VOID SetServiceParam(int service_port, int monitor_port,
                       GetCapabilityFunc get_capability);
  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen);
  VOID EventNotify(OSAL_Socket_Handle eventSock,
                   CbSocket::SOCKET_NOTIFYTYPE type);

 private:
  char name_[64];
  int query_request_count_;
  int service_port_;
  INT32 monitor_port_;
  GetCapabilityFunc get_capability_;
};

#endif
