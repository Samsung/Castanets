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

#include "discovery_client.h"

#include <sstream>
#include <string>
#include <vector>

#include "service_provider.h"

using namespace mmBase;
using namespace mmProto;

static const int kDefaultTTL = 64;
static const char kDiscoveryResponseScheme[] = "discovery-response://";

struct DiscoveryInfo {
  int service_port = 0;
  int monitor_port = 0;
  std::string capability;
  std::string request_from;
};

static DiscoveryInfo ParseResponse(const char* response) {
  std::vector<std::string> tokens;
  std::string token;
  std::stringstream ss(response);
  while (getline(ss, token, '&'))
    tokens.push_back(token);

  DiscoveryInfo info;
  for (const auto& token : tokens) {
    size_t pos = token.find("=");
    std::string key = token.substr(0, pos);
    std::string value = token.substr(pos + 1);

    if (key == "service-port") {
      info.service_port = atoi(value.c_str());
    } else if (key == "monitor-port") {
      info.monitor_port = atoi(value.c_str());
    } else if (key == "request-from") {
      info.request_from = value;
    } else if (key == "capability") {
      info.capability = value;
    }
  }
  return info;
}

CDiscoveryClient::CDiscoveryClient(bool self_discovery_enabled)
    : CpUdpClient(),
      self_discovery_enabled_(self_discovery_enabled) {}

CDiscoveryClient::CDiscoveryClient(const CHAR* msgqname,
                                   bool self_discovery_enabled)
    : CpUdpClient(msgqname),
      self_discovery_enabled_(self_discovery_enabled) {}

CDiscoveryClient::~CDiscoveryClient() {}

BOOL CDiscoveryClient::StartClient(int readperonce) {
  if (!CpUdpClient::Create()) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::Create() Fail\n");
    return false;
  }

  if (!CpUdpClient::Open()) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::Open() Fail\n");
    return false;
  }

  if (!CpUdpClient::SetTTL(kDefaultTTL)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::SetTTL() Fail\n");
    return false;
  }

  if (!CpUdpClient::Start(readperonce)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::Start() Fail\n");
    return false;
  }

  return TRUE;
}

BOOL CDiscoveryClient::StopClient() {
  CpUdpClient::Stop();
  DPRINT(COMM, DEBUG_INFO, "CpUdpClient::Stop\n");
  CpUdpClient::Close();
  DPRINT(COMM, DEBUG_INFO, "CpUdpClient::Close\n");
  return TRUE;
}

VOID CDiscoveryClient::DataRecv(OSAL_Socket_Handle iEventSock,
                                const CHAR* pszsource_addr,
                                long source_port,
                                CHAR* pData,
                                INT32 iLen) {
  DPRINT(COMM, DEBUG_INFO,
         "Receive Response - [destination Address:%s][discovery "
         "port:%ld][payload:%s]\n",
         pszsource_addr, source_port, pData);
  size_t scheme_length = strlen(kDiscoveryResponseScheme);
  if ((UINT32)iLen >= scheme_length) {
    if (!strncmp(pData, kDiscoveryResponseScheme, scheme_length)) {
      auto info = ParseResponse(pData + scheme_length);

      // Ignore response from itself
      if (!self_discovery_enabled_ && (info.request_from == pszsource_addr))
        return;

      CSTI<ServiceProvider>::getInstancePtr()->AddServiceInfo(
          pszsource_addr, info.service_port, info.capability.c_str());
      CbMessage::Send(DISCOVERY_RESPONSE_EVENT, 0, info.service_port,
                      strlen(pszsource_addr), (void*)pszsource_addr,
                      MSG_UNICAST);
    }
  }
}

VOID CDiscoveryClient::EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify:%d\n", type);
}
