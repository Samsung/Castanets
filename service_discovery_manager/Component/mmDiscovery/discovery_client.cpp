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
#include <vector>
#include <iostream>

using namespace mmBase;
using namespace mmProto;
using namespace std;

static const int kDefaultTTL = 64;
static const char kServicePort[] = "service-port";
static const char kMonitorPort[] = "monitor-port";
static const char kRequestFrom[] = "request-from";

CDiscoveryClient::CDiscoveryClient(bool self_discovery_enabled)
    : CpUdpClient(), self_discovery_enabled_(self_discovery_enabled) {}

CDiscoveryClient::CDiscoveryClient(const CHAR* msgqname,
                                   bool self_discovery_enabled)
    : CpUdpClient(msgqname), self_discovery_enabled_(self_discovery_enabled) {}

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
  if ((UINT32)iLen >= strlen(DISCOVERY_PACKET_PREFIX)) {
    if (!strncmp(pData, DISCOVERY_PACKET_PREFIX,
                 strlen(DISCOVERY_PACKET_PREFIX))) {
      discoveryInfo_t info = {{0}, -1, -1, {0}};
      strncpy(info.address, pszsource_addr, sizeof(info.address) - 1);
      t_HandlePacket(&info, pData + strlen(DISCOVERY_PACKET_PREFIX));
      // Ignore response from itself
      if (!self_discovery_enabled_ &&
          !strncmp(pszsource_addr, info.request_from,
                   strlen(info.request_from))) {
        return;
      }
      CbMessage::Send(DISCOVERY_RESPONSE_EVENT, 0, source_port, sizeof(info),
                      (void*)&info, MSG_UNICAST);
    }
  }
}

VOID CDiscoveryClient::EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify:%d\n", type);
}

VOID CDiscoveryClient::t_HandlePacket(discoveryInfo_t* info /*out*/,
                                      char* packet_string /*in*/) {
  vector<string> v;
  char* ptr = strtok(packet_string, ",");
  while (ptr != NULL) {
    string c = ptr;
    v.push_back(c);
    ptr = strtok(NULL, ",");
  }

  vector<string>::iterator it;
  for (it = v.begin(); it != v.end(); it++) {
    int index = 0;
    string result[2];

    char substring[64] = {'\0'};
    strcpy(substring, it->c_str());
    char* subptr = strtok(substring, ":");
    while (index < 2) {
      result[index] = subptr;
      index++;
      subptr = strtok(NULL, ":");
    }

    if (result[0] == kServicePort) {
      info->service_port = atoi(result[1].c_str());
    } else if (result[0] == kMonitorPort) {
      info->monitor_port = atoi(result[1].c_str());
    } else if (result[0] == kRequestFrom) {
      strncpy(info->request_from, result[1].c_str(),
              sizeof(info->request_from) - 1);
    }
  }
}
