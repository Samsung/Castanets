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

#ifndef __INCLUDE_NETWORK_SERVICE_H__
#define __INCLUDE_NETWORK_SERVICE_H__

#include "RouteTable.h"
#include "StunClient.h"
#include "pUdpServer.h"

#define DHCP_START_ADDR "10.10.10.1"
#define DEFAULT_STUN_PORT 5000

class CNetworkService : public mmProto::CpUdpServer {
 public:
  CNetworkService(const char* msgqname,
                  char* pszBindAddress,
                  unsigned short stun_port = DEFAULT_STUN_PORT);
  virtual ~CNetworkService();

  BOOL StartServer(int port, int readperonce = -1);
  BOOL StopServer();
  VOID DataRecv(OSAL_Socket_Handle iEventSock,
                const CHAR* pszsource_addr,
                long source_port,
                CHAR* pData,
                INT32 iLen);
  VOID EventNotify(OSAL_Socket_Handle eventSock,
                   CbSocket::SOCKET_NOTIFYTYPE type);

  unsigned long GetFreeAddress();

  VOID DUMP_TABLE() { m_pRoutingTable->DUMP_T(); }
  INT32 MEMDUMP_TABLE(PCHAR bucket[]);
  VOID DUMP_CHANNEL() { m_pRoutingTable->DUMP_C(); }

 private:
  CRouteTable* m_pRoutingTable;
  CStunClient* m_pStunServer;
  char* m_pszBindServerAddress;
  unsigned short m_stun_port;
};

#endif  //__INCLUDE_NETWORK_SERVICE_H__
