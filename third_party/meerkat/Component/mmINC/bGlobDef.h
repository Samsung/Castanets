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

#ifndef __INCLUDE_GLOBDEF_H__
#define __INCLUDE_GLOBDEF_H__

#ifdef _USE_WINDOWS_SOCKET_
#define _WINSOCKAPI_
#endif

#ifdef WIN32
#include <windows.h>
#endif

#ifdef LINUX
#include <pthread.h>
#endif

#define __ISF_MSG_BASE 0x6000

#define ACCEPT_SOCK_EVENT __ISF_MSG_BASE + 1
#define LISTENER_SOCK_EVENT __ISF_MSG_BASE + 2

#define __CUSTOM_MSG_BASE __ISF_MSG_BASE + 0x1000

#define DISCOVERY_QUERY_EVENT __CUSTOM_MSG_BASE + 1
#define DISCOVERY_RESPONSE_EVENT __CUSTOM_MSG_BASE + 2
#define MONITOR_RESPONSE_EVENT __CUSTOM_MSG_BASE + 3

#define TCP_SERVER_MQNAME "TcpServerSock"
#define UDP_SERVER_MQNAME "UdpServerSock"
#define TCP_CLIENT_MQNAME "TcpClientSock"
#define UDP_CLIENT_MQNAME "UdpClientSock"

#define EVENT_PROTOCOLCLIENT_HANDLER "CLIENTMSGQ"
#define CLIENT_EVENT_LISTENER 0x1024

#define SERVERWRAPPER_QNAME "ServerEventQ"
#define SERVER_EVENT_LISTENER 0x768

#define APILIST_QNAME "APIListEventQ"
#define APILIST_EVENT_LISTENER 0x263

#define SMSHANDLER_QNAME "scpHandler"
#define SMSHANDLER_EVENT_LISTENER 0x356

#define CM_TASK_NAME "ConnectionManagerQ"
#define CM_EVENT_LISTNER 0x0801

#define DEFAULT_SOCK_PORT 2584

#define IPV4_ADDR_LEN 16

typedef enum image_Type { IMG_JPG = 0, IMG_GIF, IMG_BMP, IMG_PNG } E_IMG_TYPE;

typedef struct msg_packet_Type {
  int id;
  int wParam;
  int lParam;
  int len;
  void* msgdata;
  int temp;
} MSG_PACKET, *PMSG_PACKET;

typedef struct msgqueue_list_Type {
  msg_packet_Type* msgpacket;
  msgqueue_list_Type* next;
} MSGQ_LIST, *PMSGQ_LIST;

typedef struct msgqueue_head_Type {
#ifdef WIN32
  HANDLE hMutex;
  HANDLE hEvent;
#elif defined(LINUX)
  pthread_mutex_t hMutex;
  pthread_cond_t hEvent;
#endif

  int i_waitcount;
  int i_available;
  char* queuename;
  msgqueue_list_Type* first;
  msgqueue_list_Type* last;
  msgqueue_head_Type* next;
  void* pThreadmsgIF;
} MSGQ_HEAD, *PMSGQ_HEAD;

typedef struct msgqueue_headlist_Type {
#ifdef WIN32
  HANDLE hMutex;
#elif defined(LINUX)
  pthread_mutex_t hMutex;
#endif
  msgqueue_head_Type* start;
} MSGQ_HAED_LIST, *PMSGQ_HEAD_LIST;

typedef enum msgtype_Type { MSG_UNICAST = 0, MSG_BROADCAST } E_MSG_TYPE;

template <typename T>
inline void ignore_result(const T&) {}

#endif
