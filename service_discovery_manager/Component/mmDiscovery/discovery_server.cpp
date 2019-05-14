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

#include "discovery_server.h"

using namespace mmBase;
using namespace mmProto;

BOOL CDiscoveryServer::StartServer(const CHAR* channel_address,
                                   int port,
                                   int readperonce) {
  if (!CpUdpServer::Create()) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Create() Fail\n");
    return false;
  }

  if (!CpUdpServer::Open(port)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Open() Fail\n");
    return false;
  }

  if (!CpUdpServer::Join(channel_address)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Join() Fail\n");
    return false;
  }

  if (!CpUdpServer::Start(readperonce)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Start() Fail\n");
    return false;
  }

  m_query_request_count = 0;

  DPRINT(COMM, DEBUG_INFO, "start server with [%d] port\n", port);
  return TRUE;
}

BOOL CDiscoveryServer::StopServer() {
  return TRUE;
}

VOID CDiscoveryServer::DataRecv(OSAL_Socket_Handle iEventSock,
                                const CHAR* pszsource_addr,
                                long source_port,
                                CHAR* pData,
                                INT32 iLen) {
  DPRINT(COMM, DEBUG_INFO, "Receive- from:[%s - %ld] msg:[%s]\n",
         pszsource_addr, source_port, pData);

  if (!strncmp(pData, "QUERY-SERVICE", strlen("QUERY-SERVICE"))) {
    CHAR eco_body[256] = {
        '\0',
    };
    sprintf(eco_body,
            "discovery://type:query-response,service-port:%d,monitor-port:%d",
            m_service_port, m_monitor_port);
    CpUdpServer::DataSend(pszsource_addr, eco_body, strlen(eco_body),
                          source_port);
  }

  m_query_request_count++;
  CbMessage::Send(DISCOVERY_QUERY_EVENT, m_query_request_count, source_port,
                  strlen(pszsource_addr), (void*)pszsource_addr, MSG_UNICAST);
}

VOID CDiscoveryServer::EventNotify(OSAL_Socket_Handle eventSock,
                                   CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify - form:sock[%d] event[%d]\n", eventSock,
         type);
}

VOID CDiscoveryServer::SetServiceParam(INT32 service_port, INT32 monitor_port) {
  m_service_port = service_port;
  m_monitor_port = monitor_port;
}
