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

#include "RmtServer.h"

using namespace mmBase;
using namespace mmProto;

BOOL CRmtServer::RemoteServerStart(pfReceiver receiver,
                                   int port,
                                   int readperonce) {
  DPRINT(COMM, DEBUG_INFO, "start remote server with [%d] port\n", port);
  CpUdpServer::Create();
  CpUdpServer::Open(port);
  CpUdpServer::Start(readperonce);
  m_pReceiver = receiver;
  return TRUE;
}

BOOL CRmtServer::RemoteServerStop() {
  CpUdpServer::Stop();
  CpUdpServer::Destroy();
  return TRUE;
}

VOID CRmtServer::DataRecv(OSAL_Socket_Handle iEventSock,
                          const CHAR* pszsource_addr,
                          long source_port,
                          CHAR* pData,
                          INT32 iLen) {
  m_pReceiver(0, (char*)pszsource_addr, source_port, iLen, (void*)pData);
}

VOID CRmtServer::EventNotify(OSAL_Socket_Handle eventSock,
                             CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify- form:sock[%d] event[%d]\n", eventSock,
         type);
}
