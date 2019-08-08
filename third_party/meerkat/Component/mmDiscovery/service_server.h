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

#ifndef __INCLUDE_SERVICE_SERVER_H__
#define __INCLUDE_SERVICE_SERVER_H__

#include <vector>

#include "pUdpServer.h"

class ServiceLauncher;

class CServiceServer : public mmProto::CpUdpServer {
 public:
  explicit CServiceServer(const CHAR* msgqname, const CHAR* service_path);
  virtual ~CServiceServer();

  BOOL StartServer(INT32 port, INT32 readperonce = -1);
  BOOL StopServer();

  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen);
  VOID EventNotify(OSAL_Socket_Handle iEventSock,
                   CbSocket::SOCKET_NOTIFYTYPE type);
 private:
  VOID t_HandlePacket(std::vector<char*>& argv /*out*/,
                      char* packet_string /*in*/);

  ServiceLauncher* launcher_;
};

#endif // __INCLUDE_SERVICE_SERVER_H__
