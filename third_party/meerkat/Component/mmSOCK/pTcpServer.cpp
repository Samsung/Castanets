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

#include "pTcpServer.h"

#include "string_util.h"

using namespace mmBase;

namespace mmProto {

namespace {

void InitOpenSSL() {
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

EVP_PKEY* GenerateKey() {
  BIGNUM* bn = BN_new();
  if (!BN_set_word(bn, RSA_F4)) {
    DPRINT(COMM, DEBUG_ERROR, "BN_set_word() is failed.\n");
    BN_free(bn);
    return nullptr;
  }

  RSA* rsa = RSA_new();
  if (!RSA_generate_key_ex(rsa, 2048, bn, nullptr)) {
    DPRINT(COMM, DEBUG_ERROR, "RSA_generate_key_ex() is failed.\n");
    BN_free(bn);
    RSA_free(rsa);
    return nullptr;
  }

  EVP_PKEY* pkey = EVP_PKEY_new();
  if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
    DPRINT(COMM, DEBUG_ERROR, "EVP_PKEY_assign_RSA() is failed.\n");
    BN_free(bn);
    RSA_free(rsa);
    EVP_PKEY_free(pkey);
    return nullptr;
  }

  return pkey;
}

X509* GenerateX509(EVP_PKEY* pkey) {
  X509* x509 = X509_new();
  if (!x509) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to create X509.\n");
    return nullptr;
  }

  X509_set_version(x509,2);
  ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);

  X509_set_pubkey(x509, pkey);

  X509_NAME* name = X509_get_subject_name(x509);
  X509_set_issuer_name(x509, name);

  if (!X509_sign(x509, pkey, EVP_sha1())) {
    DPRINT(COMM, DEBUG_ERROR, "X509_sign() is failed.\n");
    X509_free(x509);
    return nullptr;
  }

  return x509;
}

SSL_CTX* CreateSSLContext() {
  SSL_CTX* ctx = SSL_CTX_new(SSLv23_server_method());
  if (!ctx) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to create SSL context.\n");
    return nullptr;
  }

  EVP_PKEY* pkey = GenerateKey();
  if (!pkey) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to generate EVP_PKEY.\n");
    return nullptr;
  }

  X509* x509 = GenerateX509(pkey);
  if (!x509) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to generate X509.\n");
    EVP_PKEY_free(pkey);
    return nullptr;
  }

  if (SSL_CTX_use_certificate(ctx, x509) <= 0) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to set certificate.\n");
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return nullptr;
  }

  if (SSL_CTX_use_PrivateKey(ctx, pkey) <= 0) {
    DPRINT(COMM, DEBUG_ERROR, "Unable to set private key.\n");
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return nullptr;
  }

  return ctx;
}

}  // namespace

CpAcceptSock::CpAcceptSock(const char* pszQname)
    : mmBase::CbTask(pszQname),
      m_lpDataCallback(NULL),
      m_pListenerPtr(NULL),
      m_nReadBytePerOnce(-1),
      m_hListenerMonitor(0) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();
  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error)
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
}

CpAcceptSock::CpAcceptSock(const char* pszQname, SSL* ssl)
    : mmBase::CbTask(pszQname),
      mmBase::CbSocket(ssl),
      m_lpDataCallback(NULL),
      m_pListenerPtr(NULL),
      m_nReadBytePerOnce(-1),
      m_hListenerMonitor(0) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();
  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error)
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
}

CpAcceptSock::~CpAcceptSock() {
  __OSAL_Event_Destroy(&m_hTerminateEvent);
  __OSAL_Mutex_Destroy(&m_hTerminateMutex);
  __OSAL_Socket_DeInitEvent(m_hListenerEvent);
}

VOID CpAcceptSock::Activate(OSAL_Socket_Handle iSock,
                            VOID* pListenerPtr,
                            pNetDataCBFunc lpDataCallback,
                            INT32 nReadBytePerOnce,
                            INT32 lNetworkEvent) {
  m_hSock = iSock;
  m_lpDataCallback = lpDataCallback;
  m_pListenerPtr = pListenerPtr;

  m_hListenerMonitor = lNetworkEvent;
  __OSAL_Socket_RegEvent(m_hSock, &m_hListenerEvent, m_hListenerMonitor);
  m_nReadBytePerOnce = nReadBytePerOnce;
  StartMainLoop(NULL);
}

VOID CpAcceptSock::DeActivate() {
  __OSAL_Event_Send(&m_hTerminateEvent);
  CbTask::StopMainLoop();
}

VOID CpAcceptSock::OnReceive(OSAL_Socket_Handle iEventSock,
                             const CHAR* pszsource_address,
                             long source_port,
                             CHAR* pData,
                             INT32 iLen) {
  m_lpDataCallback(m_pListenerPtr, iEventSock, pszsource_address, source_port,
                   pData, iLen);
}

VOID CpAcceptSock::OnClose(OSAL_Socket_Handle iSock) {
  CpTcpServer* pParent = (CpTcpServer*)m_pListenerPtr;
  if (pParent != NULL) {
    pParent->Send(ACCEPT_SOCK_EVENT, iSock, CbSocket::NOTIFY_CLOSED);
  }
}

void CpAcceptSock::MainLoop(void* args) {
  MSG_PACKET Packet;
  while (ISRunning()) {
    if (CbMessage::Recv(&Packet, 10) >= 0) {
      if (Packet.id == LISTENER_SOCK_EVENT) {
        if ((CbSocket::SOCKET_NOTIFYTYPE)Packet.lParam ==
            CbSocket::NOTIFY_CLOSED) {
          CpTcpServer* pParent = (CpTcpServer*)m_pListenerPtr;
          if (pParent != NULL) {
            pParent->Send(ACCEPT_SOCK_EVENT, m_hSock, CbSocket::NOTIFY_CLOSED);
          }
        }
      }
    }
    OSAL_Event_Status net_st =
        __OSAL_Socket_WaitEvent(m_hSock, m_hListenerEvent, 100);
    if (net_st == OSAL_EVENT_WAIT_GETSIG) {
      if (__OSAL_Socket_CheckEvent(m_hSock, m_hListenerEvent, FD_READ)) {
        if (CbSocket::Recv(m_hSock, m_nReadBytePerOnce) == SOCK_READ_FAIL) {
          DPRINT(COMM, DEBUG_INFO, "Tcp Server Close Socket\n");
          break;
        }
      }
    } else {
      // Event Time out !!!
    }

    OSAL_Event_Status cmd_st =
        __OSAL_Event_Wait(&m_hTerminateMutex, &m_hTerminateEvent, 100);
    if (cmd_st == OSAL_EVENT_WAIT_GETSIG) {
      DPRINT(COMM, DEBUG_INFO, "Tcp Server Network Event Monitor Loop End\n");
      break;
    }
  }
  CbSocket::Close();
}

/**
 * @brief         Constructor
 * @remarks       Constructor
 */
CpTcpServer::CpTcpServer()
    : CbTask(TCP_SERVER_MQNAME),
      m_nReadBytePerOnce(-1),
      m_hListenerMonitor(0),
      use_ssl_(false),
      ssl_ctx_(nullptr) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();
  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error)
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
}

/**
 * @brief         Constructor
 * @remarks       Constructor
 */
CpTcpServer::CpTcpServer(const char* msgq_name)
    : CbTask(msgq_name),
      m_nReadBytePerOnce(-1),
      m_hListenerMonitor(0),
      use_ssl_(false),
      ssl_ctx_(nullptr) {
  m_hTerminateEvent = __OSAL_Event_Create();
  m_hTerminateMutex = __OSAL_Mutex_Create();
  OSAL_Socket_Return ret = __OSAL_Socket_InitEvent(&m_hListenerEvent);
  if (ret == OSAL_Socket_Error)
    DPRINT(COMM, DEBUG_ERROR, "Socket Monitor Event Init Fail!!\n");
}

/**
 * @brief         Destructor
 * @remarks       Destructor
 */
CpTcpServer::~CpTcpServer() {
  __OSAL_Event_Destroy(&m_hTerminateEvent);
  __OSAL_Mutex_Destroy(&m_hTerminateMutex);
  __OSAL_Socket_DeInitEvent(m_hListenerEvent);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpServer::Create() {
  if (!PFM_NetworkInitialize()) {
    DPRINT(COMM, DEBUG_ERROR, "Platform Network Initialize Fail\n");
    return FALSE;
  }

  if (use_ssl_) {
    DPRINT(COMM, DEBUG_INFO, "Create TCP server using SSL\n");
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
BOOL CpTcpServer::Open(INT32 iPort) {
  if (OSAL_Socket_Success !=
      CbSocket::Open(AF_INET, SOCK_STREAM, IPPROTO_TCP, ACT_TCP_SERVER)) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Open Error!!\n");
    return FALSE;
  }
  /*
          BOOL bValid=TRUE;
          if(OSAL_Socket_Success!=CbSocket::SetSocketOption(SOL_SOCKET,
     SO_REUSEADDR,(CHAR*)&bValid,sizeof(bValid)))
          {
                  DPRINT(COMM,DEBUG_INFO,"Set Socket Option[SO_REUSEADDR]
     Error!!\n");
                  //return FALSE;
          }
  */
  if (OSAL_Socket_Success != CbSocket::Bind(iPort)) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Bind Error!!\n");
    return FALSE;
  }
  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpServer::Start(INT32 iBackLog,
                        INT32 nReadBytePerOnce,
                        INT32 lNetworkEvent) {
  if (OSAL_Socket_Success != CbSocket::Listen(iBackLog)) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Listen Error!!\n");
    return FALSE;
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
BOOL CpTcpServer::Stop(OSAL_Socket_Handle iSock) {
  if (iSock == CbSocket::m_hSock) {
    __OSAL_Event_Send(&m_hTerminateEvent);
    CbTask::StopMainLoop();
  } else {
    CpAcceptSock::connection_info* pConnectionInfo = GetConnectionHandle(iSock);
    if (pConnectionInfo == NULL) {
      DPRINT(COMM, DEBUG_ERROR,
             "ERR**> There is no connection Information for [%d] socket\n",
             iSock);
      return false;
    }
    CpAcceptSock* pAcceptHandle = pConnectionInfo->pConnectionHandle;
    pAcceptHandle->Send(LISTENER_SOCK_EVENT, iSock, CbSocket::NOTIFY_CLOSED);
    // pAcceptHandle->DeActivate();
    // SAFE_DELETE(pAcceptHandle);
    // DelConnectionHandle(iSock);
  }
  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpServer::Close() {
  int nCount = m_ConnList.GetCount();
  for (int i = 0; i < nCount; i++) {
    CpAcceptSock::connection_info* pConnectionInfo = m_ConnList.GetAt(i);
    CpAcceptSock* pAcceptSockHandle = pConnectionInfo->pConnectionHandle;
    pAcceptSockHandle->DeActivate();
    SAFE_DELETE(pAcceptSockHandle);
    m_ConnList.DelAt(i);
  }

  if (ssl_ctx_)
    SSL_CTX_free(ssl_ctx_);

  return TRUE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
VOID CpTcpServer::MainLoop(VOID* args) {
  MSG_PACKET Packet;
  BOOL ev_pending = FALSE;

  while (ISRunning()) {
    if (CbMessage::Recv(&Packet, 100) >= 0) {
      if (Packet.id == ACCEPT_SOCK_EVENT) {
        if ((CbSocket::SOCKET_NOTIFYTYPE)Packet.lParam ==
            CbSocket::NOTIFY_CLOSED) {
          EventNotify(Packet.wParam, CbSocket::NOTIFY_CLOSED);

          CpAcceptSock::connection_info* pConnectionInfo =
              GetConnectionHandle((OSAL_Socket_Handle)Packet.wParam);

          if (pConnectionInfo != NULL) {
            CpAcceptSock* pAcceptHandle = pConnectionInfo->pConnectionHandle;
            pAcceptHandle->DeActivate();
            SAFE_DELETE(pAcceptHandle);
            DelConnectionHandle((OSAL_Socket_Handle)Packet.wParam);
          }
        } else if ((CbSocket::SOCKET_NOTIFYTYPE)Packet.lParam ==
                   CbSocket::NOTIFY_CONNECT) {
          ev_pending = FALSE;
          OSAL_Socket_Handle pAcceptSock;
          if (CbSocket::Accept(&pAcceptSock) == SOCK_READ_FAIL) {
            DPRINT(COMM, DEBUG_ERROR, "Tcp Server Socket Accept Error\n");
          }
          EventNotify(pAcceptSock, CbSocket::NOTIFY_ACCEPT);
        }

        else if ((CbSocket::SOCKET_NOTIFYTYPE)Packet.lParam ==
                 CbSocket::NOTIFY_ACCEPT) {
          // EventNotify(Packet.wParam,CbSocket::NOTIFY_ACCEPT);
        }
      }
    }

    if (!ev_pending) {
      OSAL_Event_Status net_st =
          __OSAL_Socket_WaitEvent(m_hSock, m_hListenerEvent, 100);
      if (net_st == OSAL_EVENT_WAIT_GETSIG) {
        if (__OSAL_Socket_CheckEvent(m_hSock, m_hListenerEvent, FD_ACCEPT)) {
          ev_pending = TRUE;
          Send(ACCEPT_SOCK_EVENT, m_hSock, CbSocket::NOTIFY_CONNECT);
          //__OSAL_Sleep(100);
          /*
                  OSAL_Socket_Handle pAcceptSock;
                  if(CbSocket::Accept(&pAcceptSock)==SOCK_READ_FAIL)
                  {
                          DPRINT(COMM,DEBUG_INFO,"Tcp Server Socket Accept
             Error\n");
                  }
          */
        }
      } else {
        // Event Time out !!!
      }
    }
    OSAL_Event_Status cmd_st =
        __OSAL_Event_Wait(&m_hTerminateMutex, &m_hTerminateEvent, 100);
    if (cmd_st == OSAL_EVENT_WAIT_GETSIG) {
      DPRINT(COMM, DEBUG_INFO, "Tcp Server Network Event Monitor Loop End\n");
      break;
    }
  }
  CbSocket::Close();
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpServer::OnAccept(OSAL_Socket_Handle iSock, CHAR* szConnectorAddr) {
  CpAcceptSock::connection_info* pTempConnectionInfo;
  int nCount = m_ConnList.GetCount();
  for (int i = 0; i < nCount; i++) {
    pTempConnectionInfo = m_ConnList.GetAt(i);
    if (pTempConnectionInfo->clientSock == iSock) {
      DPRINT(COMM, DEBUG_WARN, "Connection is Already Exist:%d\n", iSock);
      return false;
    }
  }

  DPRINT(COMM, DEBUG_INFO, "OnAccepted(%s).\n", szConnectorAddr);

  SSL* ssl = nullptr;
  if (use_ssl_) {
    ssl = SSL_new(ssl_ctx_);
    if (!ssl) {
      DPRINT(COMM, DEBUG_ERROR, "SSL_new failed.\n");
      return false;
    }
    SSL_set_fd(ssl, iSock);
    if (SSL_accept(ssl) <= 0) {
      DPRINT(COMM, DEBUG_ERROR, "SSL_accept failed.\n");
      return false;
    }
    DPRINT(COMM, DEBUG_INFO, "SSL_accepted.\n");
  }

  CpAcceptSock::connection_info* pNewConnection =
      new CpAcceptSock::connection_info;
  CHECK_ALLOC(pNewConnection);

  char name_buf[128];
  memset(name_buf, 0, sizeof(name_buf));
  snprintf(name_buf, sizeof(name_buf) - 1, "%s%d", m_szThreadName, iSock);


  CpAcceptSock* pAcceptSocket;
  if (use_ssl_)
    pAcceptSocket = new CpAcceptSock(name_buf, ssl);
  else
    pAcceptSocket = new CpAcceptSock(name_buf);
  pAcceptSocket->SetClientAddress(m_szClientAddr);
  pAcceptSocket->Activate(iSock, (VOID*)this, ReceiveCallback,
                          m_nReadBytePerOnce);

  pNewConnection->clientSock = iSock;
  if (szConnectorAddr != NULL) {
    if (strlen(szConnectorAddr) < 16) {
      mmBase::strlcpy(pNewConnection->clientAddr, szConnectorAddr,
                      sizeof(pNewConnection->clientAddr));
    } else {
      mmBase::strlcpy(pNewConnection->clientAddr, "invalid addr",
                      sizeof(pNewConnection->clientAddr));
    }
  } else {
    mmBase::strlcpy(pNewConnection->clientAddr, "invalid addr",
                    sizeof(pNewConnection->clientAddr));
  }

  pNewConnection->pConnectionHandle = pAcceptSocket;
  pNewConnection->authorized = false;

  m_ConnList.AddTail(pNewConnection);

  return true;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
INT32 CpTcpServer::DataSend(const CHAR* pAddress, CHAR* pData, INT32 iLen) {
  CpAcceptSock::connection_info* pConnectionInfo =
      GetConnectionHandle(pAddress);
  if (pConnectionInfo == NULL) {
    DPRINT(COMM, DEBUG_ERROR, "There is No Connection with %s Address\n",
           pAddress);
    return -1;
  }

  CpAcceptSock* pConnectionHandle = pConnectionInfo->pConnectionHandle;
  __ASSERT(pConnectionHandle);
  INT32 iWrite = pConnectionHandle->Write(pData, iLen);
  if (iWrite != iLen) {
    DPRINT(COMM, DEBUG_ERROR, "Socket(%d) is closed while sending data\n",
           pConnectionHandle->GetSockHandle());
    CpTcpServer::Stop(pConnectionHandle->GetSockHandle());
  }
  return iWrite;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
INT32 CpTcpServer::DataSend(OSAL_Socket_Handle iSock, CHAR* pData, INT32 iLen) {
  CpAcceptSock::connection_info* pConnectionInfo = GetConnectionHandle(iSock);
  if (pConnectionInfo == NULL) {
    DPRINT(COMM, DEBUG_ERROR, "There is No Connection with %d Socket\n", iSock);
    return -1;
  }

  CpAcceptSock* pConnectionHandle = pConnectionInfo->pConnectionHandle;
  __ASSERT(pConnectionHandle);
  INT32 iWrite = pConnectionHandle->Write(pData, iLen);
  if (iWrite != iLen) {
    DPRINT(COMM, DEBUG_ERROR, "Socket(%d) is closed while sending data\n",
           iSock);
    CpTcpServer::Stop(iSock);
  }
  return iWrite;
  // return pConnectionHandle->Write(pData,iLen);
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
CHAR* CpTcpServer::Address(OSAL_Socket_Handle iSock) {
  CpAcceptSock::connection_info* pInfo = GetConnectionHandle(iSock);
  if (pInfo == NULL)
    return NULL;
  return pInfo->clientAddr;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
CpAcceptSock::connection_info* CpTcpServer::GetConnectionHandle(
    OSAL_Socket_Handle iSock) {
  BOOL bFindConnection = FALSE;
  CpAcceptSock::connection_info* pTempConnectionInfo = NULL;
  int nCount = m_ConnList.GetCount();
  for (int i = 0; i < nCount; i++) {
    pTempConnectionInfo = m_ConnList.GetAt(i);
    if (pTempConnectionInfo->clientSock == iSock) {
      bFindConnection = TRUE;
      break;
    }
  }
  if (bFindConnection) {
    return pTempConnectionInfo;
  } else {
    DPRINT(COMM, DEBUG_ERROR, "can not find socket handle (%d)\n", iSock);
    return NULL;
  }
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
CpAcceptSock::connection_info* CpTcpServer::GetConnectionHandle(
    const CHAR* pAddress) {
  BOOL bFindConnection = FALSE;
  CpAcceptSock::connection_info* pTempConnectionInfo = NULL;
  int nCount = m_ConnList.GetCount();
  for (int i = 0; i < nCount; i++) {
    pTempConnectionInfo = m_ConnList.GetAt(i);
    if (!strcmp(pTempConnectionInfo->clientAddr, pAddress)) {
      bFindConnection = TRUE;
      break;
    }
  }
  if (bFindConnection) {
    return pTempConnectionInfo;
  } else {
    DPRINT(COMM, DEBUG_ERROR, "can not find socket handle (%s)\n", pAddress);
    return NULL;
  }
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpServer::DelConnectionHandle(OSAL_Socket_Handle iSock) {
  CpAcceptSock::connection_info* pTempConnectionInfo;
  int nCount = m_ConnList.GetCount();
  for (int i = 0; i < nCount; i++) {
    pTempConnectionInfo = m_ConnList.GetAt(i);
    if (pTempConnectionInfo->clientSock == iSock) {
      m_ConnList.DelAt(i);
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
BOOL CpTcpServer::DelConnectionHandle(const CHAR* pAddress) {
  CpAcceptSock::connection_info* pTempConnectionInfo;
  int nCount = m_ConnList.GetCount();
  for (int i = 0; i < nCount; i++) {
    pTempConnectionInfo = m_ConnList.GetAt(i);
    if (!strcmp(pTempConnectionInfo->clientAddr, pAddress)) {
      m_ConnList.DelAt(i);
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * @brief         this method is not used in this project
 * @remarks       this method is not used in this project
 */
VOID CpTcpServer::ReceiveCallback(VOID* pListener,
                                  OSAL_Socket_Handle iEventSock,
                                  const CHAR* pszsource_address,
                                  long source_port,
                                  CHAR* pData,
                                  INT32 iLen) {
  ((CpTcpServer*)pListener)
      ->DataRecv(iEventSock, pszsource_address, source_port, pData, iLen);
}

}  // namespace mmProto
