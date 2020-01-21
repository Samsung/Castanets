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

#include "pTcpClient.h"

#include "string_util.h"

using namespace mmBase;

namespace mmProto {

namespace {

void InitOpenSSL() {
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

SSL_CTX* CreateSSLContext() {
  SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
  if (!ctx) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to create SSL context.\n");
    return nullptr;
  }

  return ctx;
}

}  // namespace

/**
 * @brief         »ý¼ºÀÚ
 * @remarks       »ý¼ºÀÚ
 */
CpTcpClient::CpTcpClient()
    : CbTask(TCP_CLIENT_MQNAME),
      m_nReadBytePerOnce(-1),
      m_hListenerMonitor(0),
      server_address_{0,},
      server_port_(0),
      use_ssl_(false),
      ssl_ctx_(nullptr) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();
  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error)
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
}

/**
 * @brief         持失切
 * @remarks       持失切
 */
CpTcpClient::CpTcpClient(const CHAR* msgqname)
    : CbTask(msgqname),
      m_nReadBytePerOnce(-1),
      m_hListenerMonitor(0),
      server_address_{0,},
      server_port_(0),
      use_ssl_(false),
      ssl_ctx_(nullptr) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();
  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error)
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
}

/**
 * @brief         社瑚切.
 * @remarks       社瑚切.
 */
CpTcpClient::~CpTcpClient() {
  __OSAL_Event_Destroy(&m_hTerminateEvent);
  __OSAL_Mutex_Destroy(&m_hTerminateMutex);
  __OSAL_Socket_DeInitEvent(m_hListenerEvent);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpClient::Create() {
  if (!PFM_NetworkInitialize()) {
    DPRINT(COMM, DEBUG_ERROR, "Platform Network Initialize Fail\n");
    return FALSE;
  }

  if (use_ssl_) {
    DPRINT(COMM, DEBUG_INFO, "Create TCP client using SSL\n");
    InitOpenSSL();
    ssl_ctx_ = CreateSSLContext();
    if (!ssl_ctx_) {
      DPRINT(COMM, DEBUG_ERROR, "Returned SSL Context is NULL!!\n");
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpClient::Open(const CHAR* pAddress, INT32 iPort) {
  mmBase::strlcpy(server_address_, pAddress, sizeof(server_address_));
  server_port_ = iPort;

  if (OSAL_Socket_Success !=
      CbSocket::Open(AF_INET, SOCK_STREAM, IPPROTO_TCP, ACT_TCP_CLIENT)) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Open Error!!\n");
    return FALSE;
  }

  if (SOCK_SUCCESS != CbSocket::Connect(pAddress, iPort)) {
    DPRINT(COMM, DEBUG_ERROR, "Connect to [%s] Error!!\n", pAddress);
    CbSocket::Close();
    return FALSE;
  }

  if (use_ssl_) {
    ssl_ = SSL_new(ssl_ctx_);
    if (!ssl_) {
      DPRINT(COMM, DEBUG_ERROR, "SSL_new failed.\n");
      CbSocket::Close();
      return FALSE;
    }
    SSL_set_fd(ssl_, m_hSock);
    int ret = SSL_connect(ssl_);
    if (ret <= 0) {
      ret = SSL_get_error(ssl_, ret);
      DPRINT(COMM, DEBUG_ERROR, "SSL_connect fail. err: %d\n", ret);
      CbSocket::Close();
      return FALSE;
    }
    DPRINT(COMM, DEBUG_INFO, "SSL connected.\n");
  }

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpClient::Start(INT32 nReadPerOnce, INT32 lNetworkEvent) {
  m_hListenerMonitor = lNetworkEvent;
  __OSAL_Socket_RegEvent(m_hSock, &m_hListenerEvent, m_hListenerMonitor);

  m_nReadBytePerOnce = nReadPerOnce;

  CbTask::StartMainLoop((void*)&nReadPerOnce);

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpClient::Stop(OSAL_Socket_Handle iSock) {
  __OSAL_Event_Send(&m_hTerminateEvent);
  CbTask::StopMainLoop();

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpClient::Close() {
  if (ssl_ctx_)
    SSL_CTX_free(ssl_ctx_);
  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
VOID CpTcpClient::MainLoop(void* args) {
  while (ISRunning()) {
    OSAL_Event_Status net_st =
        __OSAL_Socket_WaitEvent(m_hSock, m_hListenerEvent, 100);
    if (net_st == OSAL_EVENT_WAIT_GETSIG) {
      if (__OSAL_Socket_CheckEvent(m_hSock, m_hListenerEvent, FD_READ)) {
        if (CbSocket::Recv(m_nReadBytePerOnce) == SOCK_READ_FAIL) {
          DPRINT(COMM, DEBUG_INFO, "TCP Client Close Socket\n");
          break;
        }
      }
    } else {
      // Event Time out !!!
    }

    OSAL_Event_Status cmd_st =
        __OSAL_Event_Wait(&m_hTerminateMutex, &m_hTerminateEvent, 100);
    if (cmd_st == OSAL_EVENT_WAIT_GETSIG) {
      DPRINT(COMM, DEBUG_INFO, "TCP Client Network Event Monitor Loop End\n");
      break;
    }
  }
  CbSocket::Close();
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
int CpTcpClient::DataSend(CHAR* pData, int iLen) {
  return Write(pData, iLen);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
VOID CpTcpClient::OnReceive(OSAL_Socket_Handle iEventSock,
                            const CHAR* pszsource_address,
                            long source_port,
                            CHAR* pData,
                            int iLen) {
  DataRecv(iEventSock, pszsource_address, source_port, pData, iLen);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
VOID CpTcpClient::OnClose(OSAL_Socket_Handle iSock) {
  EventNotify(NOTIFY_CLOSED);
}

}  // namespace mmProto
