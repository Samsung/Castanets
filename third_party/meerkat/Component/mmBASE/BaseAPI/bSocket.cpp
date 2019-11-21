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

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#endif

#include "bSocket.h"
#include "string_util.h"

using namespace mmBase;

#define MAX_DUP_COUNT 10
bool mmBase::g_InitializeNetworking = FALSE;

/**
 * @brief         Constructor.
 * @remarks       Constructor.
 */
CbSocket::CbSocket() {
  m_hSock = 0;
  m_szClintAddr = NULL;
  m_hEventmutex = __OSAL_Mutex_Create();
  m_nPort = 0;
  m_type = ACT_TCP_SERVER;
}

/**
 * @brief         Destructor.
 * @remarks       Destructor.
 */
CbSocket::~CbSocket() {}

/**
 * @brief         Open.
 * @remarks       socket instance create.
 * @param         iDomain        domain
 * @param         iType          socket type
 * @param         iProtocol      protocol
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Open(INT32 iDomain,
                                          INT32 iType,
                                          INT32 iProtocol,
                                          SOCKET_ACT type) {
  OSAL_Socket_Return ret = __OSAL_Socket_Init();
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "socket() initialize Error!!\n");
    return SOCK_CREATE_FAIL;
  }

  ret = __OSAL_Socket_Open(iDomain, iType, iProtocol, &m_hSock);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "socket() Error!!\n");
    return SOCK_CREATE_FAIL;
  }

  m_type = type;
  return SOCK_SUCCESS;
}

/**
 * @brief         Close.
 * @remarks       socket instance destroy.
 * @param         iSock        socket handle
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Close(OSAL_Socket_Handle iSock) {
  __OSAL_Mutex_Lock(&m_hEventmutex);
  OnClose(iSock);

  __OSAL_Socket_shutdown(iSock);

  OSAL_Socket_Return ret = __OSAL_Socket_Close(iSock);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "closesocket() fail\n");
    __OSAL_Mutex_UnLock(&m_hEventmutex);
    return SOCK_CLOSE_FAIL;
  }
  __OSAL_Mutex_UnLock(&m_hEventmutex);
  return SOCK_SUCCESS;
}

/**
 * @brief         Bind.
 * @remarks       binding socket
 * @param         iPort        port number
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Bind(INT32 iPort) {
  if (m_hSock <= 0) {
    return SOCK_BIND_FAIL;
  }

  OSAL_Socket_Return ret = __OSAL_Socket_Bind(m_hSock, iPort);

  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "bind() fail\n");
    return SOCK_BIND_FAIL;
  }
  m_nPort = iPort;
  return SOCK_SUCCESS;
}

CbSocket::SOCKET_ERRORCODE CbSocket::Join(const CHAR* address) {
  struct ip_mreq multicastRequest;
  multicastRequest.imr_multiaddr.s_addr = inet_addr(address);
  multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
  if (__OSAL_Socket_SetOpt(m_hSock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           (CHAR*)&multicastRequest,
                           sizeof(multicastRequest)) < 0) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Join %s Fail\n", address);
    return SOCK_PROP_FAIL;
  }
  return SOCK_SUCCESS;
}

CbSocket::SOCKET_ERRORCODE CbSocket::SetTTL(UCHAR ttl) {
  if (__OSAL_Socket_SetOpt(m_hSock, IPPROTO_IP, IP_MULTICAST_TTL, (CHAR*)&ttl,
                           sizeof(ttl)) < 0) {
    DPRINT(COMM, DEBUG_ERROR, "Socket set ttl %d Fail\n", ttl);
    return SOCK_PROP_FAIL;
  }
  return SOCK_SUCCESS;
}

/**
 * @brief         Accept.
 * @remarks       accept socket
 * @param         iSock        socket handle
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Accept(OSAL_Socket_Handle iSock,
                                            OSAL_Socket_Handle* pAcceptSock) {
  __OSAL_Mutex_Lock(&m_hEventmutex);
  OSAL_Socket_Return ret;
  OSAL_Socket_Handle newsock;
  struct sockaddr_in addr_in;
  int len = sizeof(addr_in);

  ret = __OSAL_Socket_Accept(iSock, &newsock, len, &addr_in);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "accept() Fail!!!\n");
    __OSAL_Mutex_UnLock(&m_hEventmutex);
    return SOCK_ACCEPT_FAIL;
  }

  m_szClintAddr = new CHAR[strlen(inet_ntoa(addr_in.sin_addr)) + 1];
  strlcpy(m_szClintAddr, inet_ntoa(addr_in.sin_addr), sizeof(m_szClintAddr));

  int iCount = MAX_DUP_COUNT;
  while (iCount--) {
    if ((newsock == 0) || (!OnAccept(newsock, m_szClintAddr))) {
      DPRINT(
          COMM, DEBUG_WARN,
          "==Socket Descriptor is allocated Zero. -> Try to Re-Allocate==\n");
#ifndef WIN32
      OSAL_Socket_Handle dupsock;
      dupsock = dup(newsock);
      if (dupsock < 0) {
        DPRINT(COMM, DEBUG_WARN, "socket duplicate error\n");
      } else {
        close(newsock);
        newsock = dupsock;
      }
#else
      break;
#endif

    } else {
      break;
    }
  }
  *pAcceptSock = newsock;
  __OSAL_Mutex_UnLock(&m_hEventmutex);

  return SOCK_SUCCESS;
}

/**
 * @brief         Connect.
 * @remarks       connect socket
 * @param         iSock        		socket handle
 * @param         szToConnectIP         ip address string
 * @param         iPort        		port
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Connect(OSAL_Socket_Handle iSock,
                                             const CHAR* szToConnectIP,
                                             INT32 iPort) {
  if (iSock <= 0) {
    return SOCK_CONNECT_FAIL;
  }

  OSAL_Socket_Return ret = __OSAL_Socket_Connect(iSock, szToConnectIP, iPort);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "Socket Connect Fail\n");
    return SOCK_CONNECT_FAIL;
  }
  return SOCK_SUCCESS;
}

/**
 * @brief         Listen.
 * @remarks       listen socket
 * @param         iBacklog        	listen counter
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Listen(INT32 iBacklog) {
  int iRc = listen(m_hSock, iBacklog);
  if (iRc) {
    DPRINT(COMM, DEBUG_ERROR, "listen() fail\n");
    return SOCK_LISTEN_FAIL;
  }

  return SOCK_SUCCESS;
}

/**
 * @brief         receive data form socket.
 * @remarks       receive data form socket.
 * @param         iSock        	socket handler
 * @param         nbyte        	data byte to be received
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::Recv(OSAL_Socket_Handle iSock, int nbyte) {
  __OSAL_Mutex_Lock(&m_hEventmutex);
  unsigned long nbuffered = 0, toread = 0;
  int readbyte = 0;
  OSAL_Socket_Return ret;
  ret = __OSAL_Socket_IOCTL(iSock, FIONREAD, &nbuffered);
  if (nbuffered == 0) {
    __OSAL_Mutex_UnLock(&m_hEventmutex);
    return SOCK_READ_FAIL;
  }

  if (nbyte < 0)
    toread = nbuffered;
  else {
    if ((int)nbuffered > nbyte)
      toread = nbyte;
    else
      toread = nbuffered;
  }

  char* buf = (char*)malloc(toread + 3);
  CHECK_ALLOC(buf);
  memset(buf, 0, toread + 3);

  ret = __OSAL_Socket_Recv(iSock, buf, toread, &readbyte);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_WARN, "Socket Read Fail --[Socket Already Closed??]\n");
    SAFE_DELETE(buf);
    return SOCK_READ_FAIL;
  }
  OnReceive(iSock, GetClientAddress(), m_nPort, buf, readbyte);

#ifdef __SOCK_DEBUG__
  char name[12];
  memset(name, 0, 12);
  sprintf(name, "%d.netlog", iSock);
  FILE* logptr = fopen(name, "ab");
  if (logptr != NULL) {
    fwrite(buf, 1, readbyte, logptr);
    fprintf(logptr, "\n-----------------------------------\n");
  }
  fclose(logptr);
#endif

  free(buf);
  __OSAL_Mutex_UnLock(&m_hEventmutex);
  return SOCK_SUCCESS;
}

/**
 * @brief         receive data form socket.
 * @remarks       receive data form socket.
 * @param         iSock        	socket handler
 * @param         nbyte        	data byte to be received
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::RecvFrom(OSAL_Socket_Handle iSock,
                                              int nbyte) {
  __OSAL_Mutex_Lock(&m_hEventmutex);

  unsigned long nbuffered = 0, toread = 0;
  int readbyte = 0;
  OSAL_Socket_Return ret;
  sockaddr_in senderaddr_in;
  int senderaddr_len = sizeof(senderaddr_in);

  ret = __OSAL_Socket_IOCTL(iSock, FIONREAD, &nbuffered);
  if (nbuffered == 0) {
    __OSAL_Mutex_UnLock(&m_hEventmutex);
    return SOCK_READ_FAIL;
  }

  if (nbyte < 0)
    toread = nbuffered;
  else {
#ifndef LEESS /*edit*/
    if ((int)nbuffered > nbyte) {
      toread = nbuffered;
      printf("LEESS: ==================================================\n");
      printf("Read Length: %ld\n", toread);
      printf("========================================================\n");
    } else
      toread = nbyte;
#else /*origin*/
    if ((int)nbuffered > nbyte)
      toread = nbyte;
    else
      toread = nbuffered;
#endif
  }

  char* buf = (char*)malloc(toread + 3);
  CHECK_ALLOC(buf);
  memset(buf, 0, toread + 3);
  ret = __OSAL_Socket_RecvFrom(iSock, buf, toread, senderaddr_len,
                               &senderaddr_in, &readbyte);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR,
           "Socket Read Fail -- [Socket Already Closed??]\n");
    SAFE_DELETE(buf);
    return SOCK_READ_FAIL;
  }
  char* pszsourceAddr = inet_ntoa(senderaddr_in.sin_addr);
  long lsourceport = ntohs(senderaddr_in.sin_port);
  OnReceive(iSock, pszsourceAddr, lsourceport, buf, readbyte);
  free(buf);
  __OSAL_Mutex_UnLock(&m_hEventmutex);
  return SOCK_SUCCESS;
}

/**
 * @brief         write data to socket.
 * @remarks       write data to socket
 * @param         iSock        		socket handle
 * @param         pData        		data pointer
 * @param         iLen        		data length
 * @return        SOCKET_ERRORCODE
 */
INT32 CbSocket::Write(OSAL_Socket_Handle iSock, CHAR* pData, INT32 iLen) {
  int iSentall = 0, iSent, iToSend = 0;
  while (iSentall < iLen) {
    iToSend = iLen - iSentall;
    OSAL_Socket_Return ret =
        __OSAL_Socket_Send(iSock, (char*)&pData[iSentall], iToSend, &iSent);
    if (ret == OSAL_Socket_Error) {
      DPRINT(COMM, DEBUG_ERROR, "Socket Send Fail\n");
      return iSentall;
    } else {
      iSentall = iSentall + iSent;
    }
    printf("#%d %d %d\n", iSentall, iToSend, iLen);
  }
  return iSentall;
}

/**
 * @brief         write data to socket.
 * @remarks       write data to socket
 * @param         iSock        		socket handle
 * @param         pData        		data pointer
 * @param         iLen        		data length
 * @param         szDestAddrIP        	destnation ip address
 * @param         port        		port number
 * @return        SOCKET_ERRORCODE
 */
INT32 CbSocket::WriteTo(OSAL_Socket_Handle iSock,
                        CHAR* pData,
                        INT32 iLen,
                        const CHAR* szDestAddrIP,
                        INT32 port) {
  int iSentall = 0, iSent, iToSend = 0;

  while (iSentall < iLen) {
    iToSend = iLen - iSentall;
    OSAL_Socket_Return ret = __OSAL_Socket_SendTo(
        iSock, &pData[iSentall], iToSend, szDestAddrIP, port, &iSent);

    if (ret == OSAL_Socket_Error) {
      DPRINT(COMM, DEBUG_ERROR, "Socket Send Fail\n");
      return iSentall;
    } else {
      iSentall = iSentall + iSent;
    }
  }
  return iSentall;
}

/**
 * @brief         get socket option.
 * @remarks       socket iocontrol wrapper
 * @param         iSock        	socket handle
 * @param         iLevel        option level
 * @param         iOpt        	option id
 * @param         *pOptval      option value
 * @param         iOptlen       option length
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::GetSocketOption(OSAL_Socket_Handle iSock,
                                                     INT32 iLevel,
                                                     INT32 iOpt,
                                                     CHAR* pOptval,
                                                     INT32* pOptlen) {
  OSAL_Socket_Return ret =
      __OSAL_Socket_GetOpt(iSock, iLevel, iOpt, pOptval, pOptlen);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "getsockopt() fail\n");
    return SOCK_PROP_FAIL;
  }
  return SOCK_SUCCESS;
}

/**
 * @brief         set socket option.
 * @remarks       socket iocontrol wrapper
 * @param         iSock        	socket handle
 * @param         iLevel        option level
 * @param         iOpt        	option id
 * @param         *pOptval      option value
 * @param         iOptlen       option length
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::SetSocketOption(OSAL_Socket_Handle iSock,
                                                     INT32 iLevel,
                                                     INT32 iOpt,
                                                     CHAR* pOptval,
                                                     INT32 iOptlen) {
  OSAL_Socket_Return ret =
      __OSAL_Socket_SetOpt(iSock, iLevel, iOpt, pOptval, iOptlen);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "setsockopt() fail\n");
    return SOCK_PROP_FAIL;
  }
  return SOCK_SUCCESS;
}

/**
 * @brief         SetBlockMode.
 * @remarks       Set Block,NonBlock mode
 * @param         iSock        	socket handle
 * @param         bBlock        block mode
 * @return        SOCKET_ERRORCODE
 */
CbSocket::SOCKET_ERRORCODE CbSocket::SetBlockMode(OSAL_Socket_Handle iSock,
                                                  BOOL bBlock) {
  OSAL_Socket_Return ret = __OSAL_Socket_BlockMode(iSock, bBlock);
  if (ret == OSAL_Socket_Error) {
    DPRINT(COMM, DEBUG_ERROR, "setsockopt() fail\n");
    return SOCK_PROP_FAIL;
  }
  return SOCK_SUCCESS;
}

/**
 * @brief         Initialize network enviroment
 * @remarks       Initialize network enviroment
 */
BOOL mmBase::PFM_NetworkInitialize(void) {
  if (g_InitializeNetworking)
    return TRUE;

  if (__OSAL_Socket_Init() != OSAL_Socket_Success) {
    g_InitializeNetworking = FALSE;
    DPRINT(COMM, DEBUG_ERROR, "Network Initialize Fail!!!\n");
    return FALSE;
  }

  g_InitializeNetworking = TRUE;
  DPRINT(COMM, DEBUG_INFO, "Network Initialize success\n");
  return TRUE;
}

/**
 * @brief         UnInitialize network enviroment
 * @remarks       UnInitialize network enviroment
 */
VOID mmBase::PFM_NetworkDeInitialize(void) {
  g_InitializeNetworking = FALSE;
  __OSAL_Socket_DeInit();
}
