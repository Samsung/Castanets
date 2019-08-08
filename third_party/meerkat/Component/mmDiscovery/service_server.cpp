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

CServiceServer::CServiceServer(const CHAR* msgqname, const CHAR* service_path)
    : CpUdpServer(msgqname),
      launcher_(new ServiceLauncher(service_path)) {

}

CServiceServer::~CServiceServer() {
  delete launcher_;
}

BOOL CServiceServer::StartServer(INT32 port, INT32 readperonce) {
  if (!CpUdpServer::Create()) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Create() Fail\n");
    return FALSE;
  }

  if (!CpUdpServer::Open(port)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Open() Fail\n");
    return FALSE;
  }

  if (!CpUdpServer::Start(readperonce)) {
    DPRINT(COMM, DEBUG_ERROR, "CpUdpServer::Start() Fail\n");
    return FALSE;
  }

  DPRINT(COMM, DEBUG_INFO, "Start service server with [%d] port\n", port);
  return TRUE;
}

BOOL CServiceServer::StopServer() {
  CpUdpServer::Stop();
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

  if (!strncmp(pData, "service-request://", strlen("service-request://"))) {
    std::vector<char*> argv;
    t_HandlePacket(argv, pData + strlen("service-request://"));

    if (argv.empty()) {
      argv.push_back(const_cast<char*>("_"));
      argv.push_back(const_cast<char*>("--type=renderer"));
    }

    char server_address[35] = {'\0',};
    sprintf(server_address, "--enable-castanets=%s", pszsource_addr);
    argv.push_back(server_address);

    // TODO: |server_address_old| should be remove after applying
    // https://github.com/Samsung/Castanets/pull/75.
    char server_address_old[35] = {'\0',};
    sprintf(server_address_old, "--server-address=%s", pszsource_addr);
    argv.push_back(server_address_old);

#if defined(ANDROID)
    FILE* file = fopen("/data/local/tmp/chrome-command-line", "w");
    if (!file) {
      DPRINT(COMM, DEBUG_ERROR, "chrome-command-line file open failed! - errno(%d)\n", errno);
      return;
    }

    int argc = argv.size();
    for (int i = 0; i < argc; i++) {
      fwrite(argv[i], sizeof(char), strlen(argv[i]), file);
      fwrite(" ", sizeof(char), 1, file);
    }

    fclose(file);

    Java_startChromeRenderer();
#else
    if (!launcher_->LaunchRenderer(argv))
      RAW_PRINT("Renderer launch failed!!\n");
#endif  // defined(ANDROID)
  }
}

VOID CServiceServer::EventNotify(OSAL_Socket_Handle iEventSock,
                                 CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify - form:sock[%d] event[%d]\n",
         iEventSock, type);
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
}
