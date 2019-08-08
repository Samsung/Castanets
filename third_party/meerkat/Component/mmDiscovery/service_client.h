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

#ifndef __INCLUDE_SERVICE_CLIENT_H__
#define __INCLUDE_SERVICE_CLIENT_H__

#include "pUdpClient.h"

class CServiceClient : public mmProto::CpUdpClient {
 public:
  explicit CServiceClient(const CHAR* msgqname);
  virtual ~CServiceClient();

  BOOL StartClient(int readperonce = -1);
  BOOL StopClient();
  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                int iLen);
  VOID EventNotify(CbSocket::SOCKET_NOTIFYTYPE type);
};

#endif // __INCLUDE_SERVICE_CLIENT_H__
