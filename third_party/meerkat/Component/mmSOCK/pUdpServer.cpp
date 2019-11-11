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

#include "pUdpServer.h"

using namespace mmBase;
using namespace mmProto;
/**
 * @brief         Constructor
 * @remarks       Constructor
 */
CpUdpServer::CpUdpServer() : CbTask(UDP_SERVER_MQNAME) {}

/**
 * @brief         Constructor
 * @remarks       Constructor
 */
CpUdpServer::CpUdpServer(const CHAR* msgqname) : CbTask(msgqname) {}

/**
 * @brief         Destructor
 * @remarks       Destructor
 */
CpUdpServer::~CpUdpServer() {}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpUdpServer::Create() {
  BOOL bRet = PFM_NetworkInitialize();
  if (bRet == FALSE) {
    DPRINT(COMM, DEBUG_ERROR, "Platform Network Initialize Fail\n");
  }

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpUdpServer::Open(INT32 iPort) {
  if (SOCK_SUCCESS !=
      CbSocket::Open(AF_INET, SOCK_DGRAM, IPPROTO_UDP, ACT_UDP_SERVER)) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Open Error!!\n");
    return FALSE;
  }

  if (SOCK_SUCCESS != CbSocket::Bind(iPort)) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Bind Error!!\n");
    return FALSE;
  }
  /*
          BOOL bValid=TRUE;
          if(SOCK_SUCCESS!=CbSocket::SetSocketOption(SOL_SOCKET,
     SO_REUSEADDR,(CHAR*)&bValid,sizeof(bValid)))
          {
                  DPRINT(COMM,DEBUG_INFO,"Set Socket Option[SO_REUSEADDR]
     Error!!\n");
                  return FALSE;
          }
  */
  if (SOCK_SUCCESS != CbSocket::SetBlockMode(false)) {
    DPRINT(COMM, DEBUG_ERROR, "Set Socket Blocking mode error!!\n");
    return FALSE;
  }

  return TRUE;
}

BOOL CpUdpServer::Join(const CHAR* channel_addr) {
  if (SOCK_SUCCESS != CbSocket::Join(channel_addr)) {
    return FALSE;
  }
  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpUdpServer::Start(INT32 nReadBytePerOnce, INT32 lNetworkEvent) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();

  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
  }

  m_hListenerMonitor = lNetworkEvent;
  __OSAL_Socket_RegEvent(m_hSock, &m_hListenerEvent, m_hListenerMonitor);

  m_nReadBytePerOnce = nReadBytePerOnce;

  CbTask::StartMainLoop(NULL);

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpUdpServer::Stop() {
  CbTask::StopMainLoop();
  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpUdpServer::Close() {
  __OSAL_Event_Send(&m_hTerminateEvent);
  CbTask::StopMainLoop();
  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
void CpUdpServer::MainLoop(void* args) {
  while (ISRunning()) {
    OSAL_Event_Status net_st =
        __OSAL_Socket_WaitEvent(m_hSock, m_hListenerEvent, 100);
    if (net_st == OSAL_EVENT_WAIT_GETSIG) {
      if (__OSAL_Socket_CheckEvent(m_hSock, m_hListenerEvent, FD_READ)) {
        if (CbSocket::RecvFrom(m_nReadBytePerOnce) == SOCK_READ_FAIL) {
          DPRINT(COMM, DEBUG_INFO, "UDP Server Close Socket\n");
          break;
        }
      }
    } else {
      // Event Time out !!!
    }

    OSAL_Event_Status cmd_st =
        __OSAL_Event_Wait(&m_hTerminateMutex, &m_hTerminateEvent, 100);
    if (cmd_st == OSAL_EVENT_WAIT_GETSIG) {
      DPRINT(COMM, DEBUG_INFO, "UDP Server Network Event Monitor Loop End\n");
      break;
    }
  }
  CbSocket::Close();
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
INT32 CpUdpServer::DataSend(const CHAR* pAddress, CHAR* pData, INT32 iLen) {
  return WriteTo(pData, iLen, pAddress);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
INT32 CpUdpServer::DataSend(const CHAR* pAddress,
                            CHAR* pData,
                            INT32 iLen,
                            INT32 port) {
  return WriteTo(pData, iLen, pAddress, port);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
VOID CpUdpServer::OnReceive(OSAL_Socket_Handle iEventSock,
                            const CHAR* pAddress,
                            long source_port,
                            CHAR* pData,
                            INT32 iLen) {
  DataRecv(iEventSock, pAddress, source_port, pData, iLen);

  //	ClientData(iEventSock,pAddress,pData,iLen);
}
