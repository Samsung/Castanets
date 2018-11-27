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

#include "service_server.h"

#include "service_launcher.h"

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
      argv.push_back("");
      argv.push_back("--type=renderer");
    }

    char server_address[33] = {'\0',};
    sprintf(server_address, "--server-address=%s", pszsource_addr);
    argv.push_back(server_address);

    if (!launcher_->LaunchRenderer(argv))
      RAW_PRINT("Renderer launch failed!!\n");
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
    argv.push_back(tok);
    tok = strtok(nullptr, "&");
  }
}
