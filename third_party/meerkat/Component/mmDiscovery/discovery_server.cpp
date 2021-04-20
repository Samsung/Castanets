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

CDiscoveryServer::CDiscoveryServer()
    : query_request_count_(0),
      service_port_(DEFAULT_SERVICE_PORT),
      monitor_port_(DEFAULT_MONITOR_PORT),
      get_capability_(nullptr) {
}

CDiscoveryServer::CDiscoveryServer(const CHAR* msgqname)
    : CpUdpServer(msgqname),
      query_request_count_(0),
      service_port_(DEFAULT_SERVICE_PORT),
      monitor_port_(DEFAULT_MONITOR_PORT),
      get_capability_(nullptr) {
    mmBase::strlcpy(name_, msgqname, sizeof(name_));
}

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
    CbSocket::Close();
    return false;
  }

  if (!CpUdpServer::Start(readperonce)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Start() Fail\n");
    return false;
  }

  query_request_count_ = 0;

  DPRINT(COMM, DEBUG_INFO, "Start discovery server with [%s:%d]\n",
         channel_address, port);
  return TRUE;
}

BOOL CDiscoveryServer::StopServer() {
  DPRINT(COMM, DEBUG_INFO, "Stop discovery server\n");
  CpUdpServer::Stop();
  return TRUE;
}

VOID CDiscoveryServer::DataRecv(OSAL_Socket_Handle iEventSock,
                                const CHAR* pszsource_addr,
                                long source_port,
                                CHAR* pData,
                                INT32 iLen) {
  DPRINT(COMM, DEBUG_INFO, "[Discovery] Receive- from:[%s - %ld] msg:[%s]\n",
         pszsource_addr, source_port, pData);

  if (!strncmp(pData, "QUERY-SERVICE", strlen("QUERY-SERVICE"))) {

    CHAR eco_body[2048] = {
        '\0',
    };
    std::string capability;
    if (get_capability_)
      capability = get_capability_();
    snprintf(eco_body, sizeof(eco_body) - 1,
             "discovery-response://"
             "service-port=%d&monitor-port=%d&request-from=%s&capability=%s",
             service_port_, monitor_port_, pszsource_addr, capability.c_str());
    CpUdpServer::DataSend(pszsource_addr, eco_body, strlen(eco_body),
                          source_port);
  }

  query_request_count_++;
  CbMessage::Send(DISCOVERY_QUERY_EVENT, query_request_count_, source_port,
                  strlen(pszsource_addr), (void*)pszsource_addr, MSG_UNICAST);
}

VOID CDiscoveryServer::EventNotify(OSAL_Socket_Handle eventSock,
                                   CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify - form:sock[%d] event[%d]\n", eventSock,
         type);
}

VOID CDiscoveryServer::SetServiceParam(int service_port, int monitor_port,
                                       GetCapabilityFunc get_capability) {
  service_port_ = service_port;
  monitor_port_ = monitor_port;
  get_capability_ = get_capability;
}
