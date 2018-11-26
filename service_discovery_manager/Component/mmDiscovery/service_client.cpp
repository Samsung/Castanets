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

#include "service_client.h"

using namespace mmProto;

static const UCHAR default_ttl = 64;

CServiceClient::CServiceClient(const CHAR* msgqname) : CpUdpClient(msgqname) {

}

CServiceClient::~CServiceClient() {

}

BOOL CServiceClient::StartClient(int readperonce) {
  if (!CpUdpClient::Create()) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::Create() Fail\n");
    return FALSE;
  }

  if (!CpUdpClient::Open()) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::Open() Fail\n");
    return FALSE;
  }

  if (!CpUdpClient::SetTTL(default_ttl)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::SetTTL() Fail\n");
    return FALSE;
  }

  if (!CpUdpClient::Start(readperonce)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpClient::Start() Fail\n");
    return FALSE;
  }

  return TRUE;
}

BOOL CServiceClient::StopClient() {
  CpUdpClient::Stop();
  CpUdpClient::Close();
  return TRUE;
}

VOID CServiceClient::DataRecv(OSAL_Socket_Handle iEventSock,
                              const CHAR* pszsource_addr,
                              long source_port,
                              CHAR* pData,
                              int iLen) {
  DPRINT(COMM, DEBUG_INFO,
         "Receive packet - [Source Address:%s][Source port:%ld]"
         "[Payload:%s]\n", pszsource_addr, source_port, pData);
}

VOID CServiceClient::EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify:%d\n", type);
}
