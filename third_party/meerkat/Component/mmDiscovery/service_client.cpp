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

static const char kVerifyTokenScheme[] = "verify-token://";
static const char kVerifyDoneScheme[] = "verify-done://";

CServiceClient::CServiceClient(const CHAR* msgqname,
                               GetTokenFunc get_token,
                               VerifyTokenFunc verify_token)
    : CpTcpClient(msgqname),
      get_token_(get_token),
      verify_token_(verify_token),
      state_(NONE) {
  set_use_ssl(true);
}

CServiceClient::~CServiceClient() {
  CpTcpClient::Close();
}

BOOL CServiceClient::StartClient(const char* address,
                                 int port,
                                 int read_per_once) {
  if (!CpTcpClient::Create()) {
    DPRINT(COMM, DEBUG_ERROR, "CpTcpClient::Create() Fail\n");
    return FALSE;
  }

  if (!CpTcpClient::Open(address, port)) {
    DPRINT(COMM, DEBUG_ERROR, "CpTcpClient::Open() Fail\n");
    return FALSE;
  }

  if (!CpTcpClient::Start(read_per_once)) {
    DPRINT(COMM, DEBUG_ERROR, "CpTcpClient::Start() Fail\n");
    return FALSE;
  }

  return TRUE;
}

BOOL CServiceClient::StopClient() {
  CpTcpClient::Stop();
  state_ = DISCONNECTED;
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

  if (!strncmp(pData, kVerifyTokenScheme, strlen(kVerifyTokenScheme))) {
    if (verify_token_ && verify_token_(pData + strlen(kVerifyTokenScheme))) {
      if (get_token_) {
        std::string token = get_token_();
        if (!token.empty()) {
          std::string message(kVerifyTokenScheme);
          message.append(token);
          DataSend(const_cast<char*>(message.c_str()), message.length() + 1);
          state_ = CONNECTING;
        }
      }
    }

    if (state_ != CONNECTING) {
      DPRINT(COMM, DEBUG_ERROR, "Verification failed.\n");
      StopClient();
    }
  } else if (!strncmp(pData, kVerifyDoneScheme, strlen(kVerifyDoneScheme))) {
    DPRINT(COMM, DEBUG_INFO, "Verification done.\n");
    state_ = CONNECTED;
  }
}

VOID CServiceClient::EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify:%d\n", type);

  if (type == CbSocket::NOTIFY_CLOSED)
    state_ = DISCONNECTED;
}
