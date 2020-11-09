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

#ifndef __INCLUDE_DISCOVERY_CLIENT_H__
#define __INCLUDE_DISCOVERY_CLIENT_H__

#include "pUdpClient.h"

#define DISCOVERY_PACKET_PREFIX "discovery://"

typedef struct discoveryInfo {
  CHAR address[16];
  INT32 service_port;
  INT32 monitor_port;
  CHAR request_from[16];
} discoveryInfo_t;

class CDiscoveryClient : public mmProto::CpUdpClient {
 public:
  CDiscoveryClient(bool self_discovery_enabled);
  CDiscoveryClient(const CHAR* msgqname, bool self_discovery_enabled);
  virtual ~CDiscoveryClient();

  BOOL StartClient(int readperonce = -1);
  BOOL StopClient();
  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen);
  VOID EventNotify(CbSocket::SOCKET_NOTIFYTYPE type);

 private:
  VOID t_HandlePacket(discoveryInfo_t* info /*out*/,
                      char* packet_string /*in*/);

  bool self_discovery_enabled_;
};

#endif
