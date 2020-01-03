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

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#endif

#include "socketAPI.h"
#include "Debugger.h"
#include "bDataType.h"
#include "bGlobDef.h"
#include "posixAPI.h"

#ifdef WIN32
#include <winsock2.h>
#endif
#ifdef LINUX
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/**
 * @brief        socket init
 * @remarks      socket init os abstraction api
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Init(void) {
#ifdef WIN32
  WSADATA wsaData;
  if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData)) {
    return OSAL_Socket_Error;
  }
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket Deinit
 * @remarks      socket Deinit os abstraction api
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_DeInit(void) {
#ifdef WIN32
  WSACleanup();
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket open
 * @remarks      socket open os abstraction API
 * @param        domain    	domain
 * @param        type    	socket type
 * @param        protocol    	socket protocol
 * @param        psock    	socket handle pointer
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Open(INT32 domain,
                                      INT32 type,
                                      INT32 protocol,
                                      OSAL_Socket_Handle* psock) {
  OSAL_Socket_Handle sock = socket(domain, type, protocol);
  *psock = sock;
  if (sock < 0)
    return OSAL_Socket_Error;
  else
    return OSAL_Socket_Success;
}

/**
 * @brief        socket shut down
 * @remarks      socket suut down os abstraction api
 * @param        sock    	socket handle
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_shutdown(OSAL_Socket_Handle sock) {
#ifdef WIN32
  shutdown(sock, SD_BOTH);
#else
  shutdown(sock, SHUT_RDWR);
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket close
 * @remarks      socket close os abstraction api
 * @param        sock    	socket handle
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Close(OSAL_Socket_Handle sock) {
#ifdef WIN32
  if (closesocket(sock) == SOCKET_ERROR)
    return OSAL_Socket_Error;
#elif defined(LINUX)
  if (close(sock) < 0)
    return OSAL_Socket_Error;
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket bind
 * @remarks      socket bind os abstraction api
 * @param        sock    	socket handle
 * @param        port		port number
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Bind(OSAL_Socket_Handle sock, int port) {
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));
  int iRc = bind(sock, (struct sockaddr*)&sin, sizeof(sin));
  if (iRc < 0)
    return OSAL_Socket_Error;
  return OSAL_Socket_Success;
}

/**
 * @brief        socket listen
 * @remarks      socket listen os abstraction api
 * @param        sock    	socket handle
 * @param        backlog	listen counter
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Listen(OSAL_Socket_Handle sock, int backlog) {
  int irc = listen(sock, backlog);
  if (irc) {
    return OSAL_Socket_Error;
  }
  return OSAL_Socket_Success;
}

/**
 * @brief        socket accept
 * @remarks      socket accept os abstraction api
 * @param        sock    	socket handle
 * @param        psock		socket handle pointer
 * @param        address_len	address length
 * @param        paddress_in	address structure
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Accept(OSAL_Socket_Handle sock,
                                        OSAL_Socket_Handle* psock,
                                        int address_len,
                                        sockaddr_in* paddress_in) {
  OSAL_Socket_Handle newsock;
#ifdef WIN32
  newsock = accept(sock, (struct sockaddr*)paddress_in, (int*)&address_len);
  if (newsock == SOCKET_ERROR)
    return OSAL_Socket_Error;
#elif defined(LINUX)
  newsock =
      accept(sock, (struct sockaddr*)paddress_in, (socklen_t*)&address_len);
  if (newsock < 0)
    return OSAL_Socket_Error;
#endif
  *psock = newsock;
  return OSAL_Socket_Success;
}

/**
 * @brief        socket connect
 * @remarks      socket connect os abstraction api
 * @param        sock    	socket handle
 * @param        ip		ip address string
 * @param        port		port number
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Connect(OSAL_Socket_Handle sock,
                                         const char* ip,
                                         INT32 port) {
  struct sockaddr_in sin_toconnect;

  memset(&sin_toconnect, 0, sizeof(sin_toconnect));

  sin_toconnect.sin_family = AF_INET;
  sin_toconnect.sin_port = htons(port);
  sin_toconnect.sin_addr.s_addr = inet_addr(ip);
  int retry = 0;
__SOCK_CONNECT_RETRY:
#ifdef WIN32
  int iRc =
      connect(sock, (struct sockaddr*)&sin_toconnect, sizeof(sin_toconnect));
  if (iRc == SOCKET_ERROR) {
    int iErr = WSAGetLastError();
    switch (iErr) {
      case WSAEWOULDBLOCK:
        // m_hSock is nonblocking and the connection cannot be completed
        // immediately.
        __OSAL_Sleep(100);
        retry++;
        if (retry > 100)
          return OSAL_Socket_Error;
        goto __SOCK_CONNECT_RETRY;
        break;
      case WSAEISCONN:
        // already connected;
        break;
      case WSAEINVAL:
        // m_hSock is listening sock
        break;
      default:
        break;
    }
    return OSAL_Socket_Error;
  }
#elif defined(LINUX)
  int iRc =
      connect(sock, (struct sockaddr*)&sin_toconnect, sizeof(sin_toconnect));
  if (iRc < 0) {
    if (errno == EINPROGRESS) {
      __OSAL_Sleep(100);
      retry++;
      if (retry > 100)
        return OSAL_Socket_Error;
      goto __SOCK_CONNECT_RETRY;
    } else {
      return OSAL_Socket_Error;
    }
  }
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket block mode 설정
 * @remarks      socket block mode 설정 os abstraction api
 * @param        sock    	socket handle
 * @param        bBlocking	blocking
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_BlockMode(OSAL_Socket_Handle sock,
                                           bool bBlocking) {
#if defined(LINUX)
  int OldFlags = fcntl(sock, F_GETFL);
  if (bBlocking && (fcntl(sock, F_SETFL, OldFlags & O_NONBLOCK) == -1))
    return OSAL_Socket_Error;
  else if (fcntl(sock, F_SETFL, OldFlags | O_NONBLOCK) == -1)
    return OSAL_Socket_Error;
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket ioctl
 * @remarks      socket ioctl os abstraction api
 * @param        sock    	socket handle
 * @param        cmd		command
 * @param        argp		argument
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_IOCTL(OSAL_Socket_Handle sock,
                                       long cmd,
                                       unsigned long* argp) {
  int ret;
#ifdef WIN32
  ret = ioctlsocket(sock, cmd, argp);
#elif defined(LINUX)
  ret = ioctl(sock, cmd, argp);
#else
  ret = -1;
#endif

  if (ret < 0)
    return OSAL_Socket_Error;
  else
    return OSAL_Socket_Success;
}

/**
 * @brief        socket receive
 * @remarks      socket receive os abstraction api
 * @param        sock    	socket handle
 * @param        buf		data buffer pointer
 * @param        toread		to read length
 * @param        pnread		real read length
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Recv(OSAL_Socket_Handle sock,
                                      char* buf,
                                      unsigned long toread,
                                      int* pnread) {
  int ret = 0;
  ret = recv(sock, buf, toread, 0);
  if (ret == 0)
    return OSAL_Socket_Error; /*socket is closed by the other side*/
  if (ret < 0)
    *pnread = 0;
  else
    *pnread = ret;
  return OSAL_Socket_Success;
}

/**
 * @brief        socket receive
 * @remarks      socket receive os abstraction api
 * @param        sock    	socket handle
 * @param        buf		data buffer pointer
 * @param        toread		to read length
 * @param        address_len	address length
 * @param        paddress_in	address structure
 * @param        pnread		real read length
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_RecvFrom(OSAL_Socket_Handle sock,
                                          char* buf,
                                          unsigned long toread,
                                          int address_len,
                                          sockaddr_in* paddress_in,
                                          int* pnread) {
  int ret = 0;

  ret = recvfrom(sock, buf, toread, 0, (sockaddr*)paddress_in,
                 (socklen_t*)&address_len);
  if (ret == 0)
    return OSAL_Socket_Error; /*socket is closed by the other side*/

  if (ret < 0)
    *pnread = 0;
  else
    *pnread = ret;

  return OSAL_Socket_Success;
}

/**
 * @brief        socket send
 * @remarks      socket send os abstraction api
 * @param        data    	data pointer
 * @param        len    	data length
 * @param        psent    	real sent data
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_Send(OSAL_Socket_Handle sock,
                                      char* data,
                                      int len,
                                      int* psent) {
  int sent = send(sock, data, len, MSG_NOSIGNAL);
#ifdef WIN32
  if (sent == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK)
      return OSAL_Socket_Error;
    *psent = 0;
  }

  else
    *psent = sent;
#elif defined(LINUX)
  if (sent < 0) {
    // TODO: if errorno is not EAGAIN return error.
    *psent = 0;
    if (errno != EAGAIN)
      return OSAL_Socket_Error;
  } else {
    *psent = sent;
  }
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket send
 * @remarks      socket send os abstraction api
 * @param        data    	data pointer
 * @param        len    	data length
 * @param        ip  	  	ip address string
 * @param        port    	port number
 * @param        psent    	real sent data
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_SendTo(OSAL_Socket_Handle sock,
                                        char* data,
                                        int len,
                                        const char* ip,
                                        int port,
                                        int* psent) {
  sockaddr_in sin_sendto;
  memset(&sin_sendto, 0, sizeof(sin_sendto));
  sin_sendto.sin_family = AF_INET;
  sin_sendto.sin_addr.s_addr = inet_addr(ip);
  sin_sendto.sin_port = htons((unsigned short)port);

  int sent = sendto(sock, data, len, 0, (struct sockaddr*)&sin_sendto,
                    sizeof(sin_sendto));
#ifdef WIN32
  if (sent == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK)
      return OSAL_Socket_Error;
    *psent = 0;
  }

  else
    *psent = sent;
#elif defined(LINUX)
  if (sent < 0) {
    // TODO: if errorno is not EAGAIN return error.
    *psent = 0;
    if (errno != EAGAIN)
      return OSAL_Socket_Error;
  } else
    *psent = sent;
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket get opt
 * @remarks      socket get opt os abstraction api
 * @param        sock    	socket handle
 * @param        level    	level
 * @param        opt  	  	option val
 * @param        poptval    	option value pointer
 * @param        poptlen   	opt struct len
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_GetOpt(OSAL_Socket_Handle sock,
                                        INT32 level,
                                        INT32 opt,
                                        CHAR* poptval,
                                        INT32* poptlen) {
  int rc;
#ifdef WIN32
  rc = getsockopt(sock, level, opt, poptval, poptlen);
  if (rc) {
    return OSAL_Socket_Error;
  }
#elif defined(LINUX)
  rc = getsockopt(sock, level, opt, poptval, (socklen_t*)poptlen);
  if (rc) {
    return OSAL_Socket_Error;
  }
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket set opt
 * @remarks      socket set opt os abstraction api
 * @param        sock    	socket handle
 * @param        level    	level
 * @param        opt  	  	option val
 * @param        poptval    	option value pointer
 * @param        poptlen   	opt struct len
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_SetOpt(OSAL_Socket_Handle sock,
                                        INT32 level,
                                        INT32 opt,
                                        CHAR* poptval,
                                        INT32 optlen) {
  int rc;
#ifdef WIN32
  rc = setsockopt(sock, level, opt, poptval, optlen);
  if (rc) {
    return OSAL_Socket_Error;
  }
#elif defined(LINUX)
  rc = setsockopt(sock, level, opt, poptval, (socklen_t)optlen);
  if (rc) {
    return OSAL_Socket_Error;
  }
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket init event
 * @remarks      socket init event os abstraction api
 * @param        pObj    	object pointer
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_InitEvent(OSAL_Socket_EventObj* pObj) {
#ifdef WIN32
  *pObj = WSACreateEvent();
#elif defined(LINUX)
  FD_ZERO(pObj);
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket Deinit event
 * @remarks      socket Deinit event os abstraction api
 * @param        pObj    	object pointer
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_DeInitEvent(OSAL_Socket_EventObj Obj) {
#ifdef WIN32
  CloseHandle(Obj);
#elif defined(LINUX)
  FD_ZERO(&Obj);
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket register event
 * @remarks      socket register event os abstraction api
 * @param        sock    	socket handle
 * @param        pObj    	object pointer
 * @param        eventtype    	event type
 * @return       OSAL_Socket_Return
 */
OSAL_Socket_Return __OSAL_Socket_RegEvent(OSAL_Socket_Handle sock,
                                          OSAL_Socket_EventObj* pObj,
                                          int eventtype) {
#ifdef WIN32
  WSAEventSelect(sock, *pObj, eventtype);
#elif defined(LINUX)
  FD_SET(sock, pObj);
#endif
  return OSAL_Socket_Success;
}

/**
 * @brief        socket wait event
 * @remarks      socket wait event os abstraction api
 * @param        sock    	socket handle
 * @param        pObj    	object pointer
 * @param        eventtype    	event type
 * @return       OSAL_Socket_Return
 */
OSAL_Event_Status __OSAL_Socket_WaitEvent(OSAL_Socket_Handle sock,
                                          OSAL_Socket_EventObj obj,
                                          int msec) {
#ifdef WIN32
  DWORD dwObjRet;
  if (msec < 0)
    dwObjRet = WaitForSingleObject(obj, INFINITE);
  else
    dwObjRet = WaitForSingleObject(obj, msec);
  if (dwObjRet == WAIT_TIMEOUT)
    return OSAL_EVENT_WAIT_TIMEOUT;
  else
    return OSAL_EVENT_WAIT_GETSIG;
#elif defined(LINUX)
  int ret;
  int selnum = sock + 1;
  struct timeval timeout;
  timeout.tv_sec = (msec / 1000);
  timeout.tv_usec = (msec % 1000) * 1000;
  ret = select(selnum, &obj, NULL, NULL, &timeout);
  if (ret > 0)
    return OSAL_EVENT_WAIT_GETSIG;
  else if (ret == 0)
    return OSAL_EVENT_WAIT_TIMEOUT;
  else
    return OSAL_EVENT_WAIT_ERROR;
#endif
}

/**
 * @brief        socket check event
 * @remarks      socket check event os abstraction api
 * @param        sock    	socket handle
 * @param        pObj    	object pointer
 * @param        eventtype    	event type
 * @return       OSAL_Socket_Return
 */
bool __OSAL_Socket_CheckEvent(OSAL_Socket_Handle sock,
                              OSAL_Socket_EventObj obj,
                              int eventtype) {
#ifdef WIN32
  WSANETWORKEVENTS event;
  WSAEnumNetworkEvents(sock, obj, &event);
  if (event.lNetworkEvents & eventtype)
    return true;
  else
    return false;
#elif defined(LINUX)
  if (FD_ISSET(sock, &obj))
    return true;
  else
    return false;
#endif
}
