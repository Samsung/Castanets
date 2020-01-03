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

#ifdef WIN32

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif

#include "service_server.h"

#include <errno.h>

#include "service_launcher.h"

#if defined(ANDROID)
#include "server_runner_jni.h"
#endif

using namespace mmProto;

static const char kServiceRequestScheme[] = "service-request://";
static const char kVerifyTokenScheme[] = "verify-token://";
static const char kVerifyDoneScheme[] = "verify-done://";

CServiceServer::CServiceServer(const CHAR* msgqname,
                               const CHAR* service_path,
                               GetTokenFunc get_token,
                               VerifyTokenFunc verify_token)
    : CpTcpServer(msgqname),
      get_token_(get_token),
      verify_token_(verify_token),
      launcher_(new ServiceLauncher(service_path)) {
  set_use_ssl(true);
}

CServiceServer::~CServiceServer() {
  delete launcher_;
  CpTcpServer::Close();
}

BOOL CServiceServer::StartServer(INT32 port, INT32 readperonce) {
  if (!CpTcpServer::Create()) {
    DPRINT(COMM, DEBUG_ERROR, "CpTcpServer::Create() Fail\n");
    return FALSE;
  }

  if (!CpTcpServer::Open(port)) {
    DPRINT(COMM, DEBUG_ERROR, "CpTcpServer::Open() Fail\n");
    return FALSE;
  }

  if (!CpTcpServer::Start(readperonce)) {
    DPRINT(COMM, DEBUG_ERROR, "CpTcpServer::Start() Fail\n");
    return FALSE;
  }

  DPRINT(COMM, DEBUG_INFO, "Start service server with [%d] port\n", port);
  return TRUE;
}

BOOL CServiceServer::StopServer() {
  CpTcpServer::Stop();
  return TRUE;
}

VOID CServiceServer::DataRecv(OSAL_Socket_Handle iEventSock,
                              const CHAR* pszsource_addr,
                              long source_port,
                              CHAR* pData,
                              INT32 iLen) {
  DPRINT(COMM, DEBUG_INFO,
         "Receive - [Source Address:%s][Source port:%ld]"
         "[Payload:%s]\n", pszsource_addr, source_port, pData);
  if (!strncmp(pData, kVerifyTokenScheme, strlen(kVerifyTokenScheme))) {
    if (verify_token_ && verify_token_(pData + strlen(kVerifyTokenScheme))) {
      CpAcceptSock::connection_info* info = GetConnectionHandle(iEventSock);
      if (info)
        info->authorized = true;
      std::string message(kVerifyDoneScheme);
      DataSend(iEventSock, const_cast<char*>(message.c_str()), message.length() + 1);
    } else {
      DPRINT(COMM, DEBUG_ERROR, "Invalid token.\n");
      CpTcpServer::Stop(iEventSock);
    }
  } else if (!strncmp(pData, kServiceRequestScheme, strlen(kServiceRequestScheme))) {
    CpAcceptSock::connection_info* info = GetConnectionHandle(iEventSock);
    if (!info || !info->authorized) {
      DPRINT(COMM, DEBUG_ERROR,
             "Service request from unauthorized client(%s)!\n", pszsource_addr);
      return;
    }

    std::vector<char*> argv;
    t_HandlePacket(argv, pData + strlen(kServiceRequestScheme));

    char server_address[35] = {'\0',};
    snprintf(server_address, sizeof(server_address) - 1,
             "--enable-castanets=%s", pszsource_addr);
    argv.push_back(server_address);

    // TODO: |server_address_old| should be remove after applying
    // https://github.com/Samsung/Castanets/pull/75.
    char server_address_old[35] = {'\0',};
    snprintf(server_address_old, sizeof(server_address_old) - 1,
             "--server-address=%s", pszsource_addr);
    argv.push_back(server_address_old);

#if defined(ANDROID)
    if (!Java_startCastanetsRenderer(argv))
#else
    if (!launcher_->LaunchRenderer(argv))
#endif  // defined(ANDROID)
	  DPRINT(COMM, DEBUG_ERROR, "Renderer launch failed!!\n");
  }
}

VOID CServiceServer::EventNotify(OSAL_Socket_Handle iEventSock,
                                 CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify - form:sock[%d] event[%d]\n",
         iEventSock, type);

  if (type == CbSocket::NOTIFY_ACCEPT) {
    // Start to exchange ID token with client.
    if (get_token_) {
      const std::string token = get_token_();
      if (!token.empty()) {
        std::string message(kVerifyTokenScheme);
        message.append(token);
        DataSend(iEventSock, const_cast<char*>(message.c_str()),
                 message.length() + 1);
      }
    }
  }
}

VOID CServiceServer::t_HandlePacket(std::vector<char*>& argv /*out*/,
                                    char* packet_string /*in*/) {
  char* tok = strtok(packet_string, "&");
  while (tok) {
    if (strncmp(tok, "--enable-castanets",
                strlen("--enable-castanets")) != 0) {
      argv.push_back(tok);
    }
    tok = strtok(nullptr, "&");
  }

  if (argv.empty()) {
    argv.push_back(const_cast<char*>("_"));
    argv.push_back(const_cast<char*>("--type=renderer"));
  }
}
