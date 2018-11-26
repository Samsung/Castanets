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

#ifndef __INCLUDE_SOCKETAPI_H__
#define __INCLUDE_SOCKETAPI_H__

#include "posixAPI.h"
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(LINUX)
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#define OSAL_Socket_Return int
#define OSAL_Socket_Error -1
#define OSAL_Socket_Success 0

#ifdef WIN32
#define OSAL_Socket_Handle SOCKET
#define OSAL_Socket_EventObj HANDLE
1234
#elif defined(LINUX)
#define OSAL_Socket_Handle int
#define OSAL_Socket_EventObj fd_set
#define FD_READ 0x1 << 0
#define FD_WRITE 0x1 << 1
#define FD_OOB 0x1 << 2
#define FD_ACCEPT 0x1 << 3
#define FD_CONNECT 0x1 << 4
#define FD_CLOSE 0x1 << 5
#endif

    // return value ret<0:Error, ret>=0:success
    OSAL_Socket_Return
    __OSAL_Socket_Open(/*[IN]*/ int domain,
                       /*[IN]*/ int type,
                       /*[IN]*/ int protocol,
                       /*[OUT]*/ OSAL_Socket_Handle* psock);
OSAL_Socket_Return __OSAL_Socket_shutdown(OSAL_Socket_Handle sock);
OSAL_Socket_Return __OSAL_Socket_Close(/*[IN]*/ OSAL_Socket_Handle sock);
OSAL_Socket_Return __OSAL_Socket_Bind(OSAL_Socket_Handle sock, int port);
OSAL_Socket_Return __OSAL_Socket_Listen(OSAL_Socket_Handle sock, int backlog);
OSAL_Socket_Return __OSAL_Socket_Accept(/*[IN]*/ OSAL_Socket_Handle sock,
                                        /*[OUT]*/ OSAL_Socket_Handle* psock,
                                        /*[IN]*/ int address_len,
                                        /*[OUT]*/ sockaddr_in* paddress_in);
OSAL_Socket_Return __OSAL_Socket_Connect(OSAL_Socket_Handle sock,
                                         const char* ip,
                                         int port);

OSAL_Socket_Return __OSAL_Socket_IOCTL(OSAL_Socket_Handle sock,
                                       long cmd,
                                       unsigned long* argp);

OSAL_Socket_Return __OSAL_Socket_BlockMode(OSAL_Socket_Handle sock,
                                           bool bBlocking);

OSAL_Socket_Return __OSAL_Socket_SendTo(OSAL_Socket_Handle sock,
                                        char* data,
                                        int len,
                                        const char* ip,
                                        int port,
                                        /*[OUT]*/ int* psent);
OSAL_Socket_Return __OSAL_Socket_Send(OSAL_Socket_Handle sock,
                                      char* data,
                                      int len,
                                      /*[OUT]*/ int* psent);
OSAL_Socket_Return __OSAL_Socket_RecvFrom(OSAL_Socket_Handle sock,
                                          char* buf,
                                          unsigned long toread,
                                          int address_len,
                                          /*[OUT]*/ sockaddr_in* paddress_in,
                                          /*[OUT]*/ int* pnread);
OSAL_Socket_Return __OSAL_Socket_Recv(OSAL_Socket_Handle sock,
                                      char* buf,
                                      unsigned long toread,
                                      /*[OUT]*/ int* pnread);

OSAL_Socket_Return __OSAL_Socket_GetOpt(OSAL_Socket_Handle sock,
                                        int level,
                                        int opt,
                                        char* poptval,
                                        int* poptlen);
OSAL_Socket_Return __OSAL_Socket_SetOpt(OSAL_Socket_Handle sock,
                                        int level,
                                        int opt,
                                        char* poptval,
                                        int optlen);

OSAL_Socket_Return __OSAL_Socket_InitEvent(OSAL_Socket_EventObj* pObj);
OSAL_Socket_Return __OSAL_Socket_DeInitEvent(OSAL_Socket_EventObj Obj);
OSAL_Socket_Return __OSAL_Socket_RegEvent(OSAL_Socket_Handle sock,
                                          OSAL_Socket_EventObj* pObj,
                                          int eventtype);
OSAL_Event_Status __OSAL_Socket_WaitEvent(OSAL_Socket_Handle sock,
                                          OSAL_Socket_EventObj obj,
                                          int msec);
bool __OSAL_Socket_CheckEvent(OSAL_Socket_Handle sock,
                              OSAL_Socket_EventObj obj,
                              int eventtype);

OSAL_Socket_Return __OSAL_Socket_Init();
OSAL_Socket_Return __OSAL_Socket_DeInit();

#endif
