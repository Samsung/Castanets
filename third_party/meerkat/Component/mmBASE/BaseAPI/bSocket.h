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

#ifndef __INCLUDE_COMMON_SOCKET_H__
#define __INCLUDE_COMMON_SOCKET_H__

#include "socketAPI.h"
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "posixAPI.h"
#include "Debugger.h"
#include "bDataType.h"

#define MAX_PROC_NUM 128
#define MAX_SZADDR_LEN 16

#define SOCK_CHECK_RETURN_NULL(x) \
  if (x != SOCK_SUCCESS) {        \
    return;                       \
  }
#define SOCK_CHECK_RETURN(x) \
  if (x != SOCK_SUCCESS) {   \
    return x;                \
  }
#ifndef IN_CLASSD
#define IN_CLASSD(i) (((long)(i)&0xf0000000) == 0xe0000000)
#endif

#ifndef IN_MULTICAST
#define IN_MULTICAST(i) IN_CLASSD(i)
#endif

namespace mmBase {

class CbSocket {
 public:
  enum SOCKET_ERRORCODE {
    SOCK_SUCCESS = 0,
    SOCK_CREATE_FAIL,
    SOCK_CLOSE_FAIL,
    SOCK_BIND_FAIL,
    SOCK_LISTEN_FAIL,
    SOCK_CONNECT_FAIL,
    SOCK_ACCEPT_FAIL,
    SOCK_PROP_FAIL,
    SOCK_READ_FAIL,
    SOCK_INVALID_VALUE
  };

  enum PROTOCOL_TYPE { AF_INET_UDP = 0, AF_INET_TCP };

  enum SOCKET_PROPERTY {
    COMMON_ADDRESS_REUSE = 0,
    COMMON_RECVBUF_SIZE,
    MCAST_SETTING_TTL,
    MCAST_SETTING_LOOP,
    MCAST_MEMBER_JOIN,
    MCAST_MEMBER_DROP
  };

  enum PAYLOAD_TYPE { RAW_UDP = 0, RAW_TCP, RTP_UDP, RTP_TCP };

  enum SOCKET_ACT {
    ACT_TCP_SERVER = 0,
    ACT_TCP_CLIENT,
    ACT_UDP_SERVER,
    ACT_UDP_CLIENT
  };

  enum SOCKET_NOTIFYTYPE {
    NOTIFY_CLOSED = 0,
    NOTIFY_CONNECT,
    NOTIFY_ACCEPT,
    NOTIFY_ERROR,
    NOTIFY_MAX
  };

 public:
  SOCKET_ERRORCODE Open(INT32 iDomain,
                        INT32 iType,
                        INT32 iProtocol,
                        SOCKET_ACT act);
  SOCKET_ERRORCODE Close() { return Close(m_hSock); }
  SOCKET_ERRORCODE Close(OSAL_Socket_Handle iSock);

  SOCKET_ERRORCODE Bind(INT32 iPort);
  SOCKET_ERRORCODE Join(const CHAR* address);
  SOCKET_ERRORCODE SetTTL(UCHAR ttl);

  SOCKET_ERRORCODE Listen(INT32 iBacklog = SOMAXCONN);

  SOCKET_ERRORCODE Accept(OSAL_Socket_Handle* pAcceptSock) {
    return Accept(m_hSock, pAcceptSock);
  }
  SOCKET_ERRORCODE Accept(OSAL_Socket_Handle iSock,
                          OSAL_Socket_Handle* pAcceptSock);
  SOCKET_ERRORCODE Connect(const CHAR* szToConnectIP, INT32 iPort) {
    return Connect(m_hSock, szToConnectIP, iPort);
  }
  SOCKET_ERRORCODE Connect(OSAL_Socket_Handle iSock,
                           const CHAR* szToConnectIP,
                           INT32 iPort);

  SOCKET_ERRORCODE Recv() { return Recv(m_hSock, -1); }
  SOCKET_ERRORCODE Recv(int nbyte) { return Recv(m_hSock, nbyte); }
  SOCKET_ERRORCODE Recv(OSAL_Socket_Handle iSock, int nbyte);

  SOCKET_ERRORCODE RecvFrom() { return RecvFrom(m_hSock, -1); }
  SOCKET_ERRORCODE RecvFrom(int nbyte) { return RecvFrom(m_hSock, nbyte); }
  SOCKET_ERRORCODE RecvFrom(OSAL_Socket_Handle iSock, int nbyte);

  INT32 Write(CHAR* pData, INT32 iLen) { return Write(m_hSock, pData, iLen); }
  INT32 Write(OSAL_Socket_Handle iSock, CHAR* pData, INT32 iLen);

  INT32 WriteTo(CHAR* pData, INT32 iLen, const CHAR* szDestAddrIP) {
    return WriteTo(m_hSock, pData, iLen, szDestAddrIP, m_nPort);
  }
  INT32 WriteTo(CHAR* pData, INT32 iLen, const CHAR* szDestAddrIP, INT32 port) {
    return WriteTo(m_hSock, pData, iLen, szDestAddrIP, port);
  }
  INT32 WriteTo(OSAL_Socket_Handle iSock,
                CHAR* pData,
                INT32 iLen,
                const CHAR* szDestAddrIP,
                INT32 port);

  SOCKET_ERRORCODE GetSocketOption(INT32 iLevel,
                                   INT32 iOpt,
                                   CHAR* pOptval,
                                   INT32* pOptlen) {
    return GetSocketOption(m_hSock, iLevel, iOpt, pOptval, pOptlen);
  }
  SOCKET_ERRORCODE GetSocketOption(OSAL_Socket_Handle iSock,
                                   INT32 iLevel,
                                   INT32 iOpt,
                                   CHAR* pOptval,
                                   INT32* pOptlen);
  SOCKET_ERRORCODE SetSocketOption(INT32 iLevel,
                                   INT32 iOpt,
                                   CHAR* pOptval,
                                   INT32 iOptlen) {
    return SetSocketOption(m_hSock, iLevel, iOpt, pOptval, iOptlen);
  }
  SOCKET_ERRORCODE SetSocketOption(OSAL_Socket_Handle iSock,
                                   INT32 iLevel,
                                   INT32 iOpt,
                                   CHAR* pOptval,
                                   INT32 iOptlen);

  SOCKET_ERRORCODE SetBlockMode(BOOL bBlock) {
    return SetBlockMode(m_hSock, bBlock);
  }
  SOCKET_ERRORCODE SetBlockMode(OSAL_Socket_Handle iSock, BOOL bBlock);
  CHAR* GetClientAddress() { return m_szClintAddr; }

  virtual VOID OnReceive(OSAL_Socket_Handle iEventSock,
                         const CHAR* pszAddress,
                         long source_port,
                         CHAR* pData,
                         INT32 iLen) = 0;
  virtual BOOL OnAccept(OSAL_Socket_Handle iSock, CHAR* szConnectorAddr) {
    return TRUE;
  }
  virtual VOID OnClose(OSAL_Socket_Handle iSock) = 0;

  CbSocket();
  virtual ~CbSocket();

 public:
  OSAL_Socket_Handle m_hSock;
  OSAL_Mutex_Handle m_hEventmutex;
  INT32 m_nPort;
  CHAR* m_szClintAddr;
  SOCKET_ACT m_type;
};

BOOL PFM_NetworkInitialize(void);
void PFM_NetworkDeInitialize(void);

extern bool g_InitializeNetworking;
}
#endif

/***********************************************************************************
----------------------------------------------------------
Usage :
----------------------------------------------------------

***********************************************************************************/
