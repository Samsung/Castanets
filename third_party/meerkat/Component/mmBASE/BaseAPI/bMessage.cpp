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
#include <crtdbg.h>
#endif

#include "Debugger.h"
#include "bMessage.h"
#include "posixAPI.h"
#include "string_util.h"

using namespace mmBase;

static MSGQ_HAED_LIST g_MsqQHeader = {__OSAL_Mutex_Create(), NULL};

/**
 * @brief         ������
 * @remarks       ������
 */
CbMessage::CbMessage() {
  m_iMQhandle = MQ_INVALIDHANDLE;
}

/**
 * @brief         ������
 * @remarks       ������
 * @param         name        message queue name
 */
CbMessage::CbMessage(const char* name) {
  m_iMQhandle = MQ_INVALIDHANDLE;
  if (name != NULL) {
    if (strlen(name) < MQ_MAXNAMELENGTH) {
      strlcpy(m_szMQname, name, sizeof(m_szMQname));
      CreateMsgQueue(name);
    } else {
      DPRINT(COMM, DEBUG_FATAL, "MsgQueue Create Fail--Too long Queue Name\n");
    }
  } else
    DPRINT(COMM, DEBUG_FATAL, "MsgQueue Create Fail--Queue Name is NULL\n");
}

/**
 * @brief         �Ҹ���.
 * @remarks       �Ҹ���.
 */
CbMessage::~CbMessage() {
  if (MQ_INVALIDHANDLE != m_iMQhandle)
    DestroyMsgQueue();
}

/**
 * @brief         Message queue create.
 * @remarks       message queue�� �����ϰ� linked list ���·� global structure��
 * attach
 * @param         name        message queue name
 * @return        success : 0, fail : -1
 */
int CbMessage::CreateMsgQueue(const char* name) {
  if (MQ_INVALIDHANDLE != m_iMQhandle)
    return -1;
  PMSGQ_HEAD pMQHead;

  __OSAL_Mutex_Lock(&g_MsqQHeader.hMutex);
  pMQHead = g_MsqQHeader.start;
  while (pMQHead != NULL && (strcmp(pMQHead->queuename, name))) {
    pMQHead = pMQHead->next;
  }
  if (pMQHead != NULL) {
    __OSAL_Mutex_UnLock(&g_MsqQHeader.hMutex);
    DPRINT(COMM, DEBUG_FATAL, "Mesage Queue already Exist--%s\n",
           pMQHead->queuename);
    return -1;
  }

  pMQHead = (PMSGQ_HEAD)malloc(sizeof(MSGQ_HEAD));
  CHECK_ALLOC(pMQHead)

  pMQHead->queuename = (char*)malloc(sizeof(char) * (strlen(name) + 1));

  CHECK_ALLOC(pMQHead->queuename)

  pMQHead->hMutex = __OSAL_Mutex_Create();
  pMQHead->hEvent = __OSAL_Event_Create();

  pMQHead->i_waitcount = 0;
  pMQHead->i_available = 0;
  strlcpy(pMQHead->queuename, name, strlen(name) + 1);
  pMQHead->first = NULL;
  pMQHead->last = NULL;
  pMQHead->pThreadmsgIF = (void*)this;
  pMQHead->next = g_MsqQHeader.start;
  g_MsqQHeader.start = pMQHead;
  __OSAL_Mutex_UnLock(&g_MsqQHeader.hMutex);
  m_iMQhandle = pMQHead;

  return 0;
}

/**
 * @brief         Message queue destroy.
 * @remarks       message queue structure�� free�ϰ� linked-list���� �����Ѵ�.
 * @return        success : 0, fail : -1
 */
int CbMessage::DestroyMsgQueue(void) {
  if (MQ_INVALIDHANDLE == m_iMQhandle) {
    DPRINT(COMM, DEBUG_FATAL, "Message Queue already destroyed\n");
    return -1;
  }

  PMSGQ_HEAD pList = (PMSGQ_HEAD)m_iMQhandle;
  PMSGQ_HEAD scanptr;
  PMSGQ_HEAD scanprevptr;
  PMSGQ_LIST travptr;
  PMSGQ_LIST travprevptr;

  __OSAL_Mutex_Lock(&g_MsqQHeader.hMutex);
  scanptr = g_MsqQHeader.start;
  scanprevptr = NULL;
  while (scanptr != pList && scanptr != NULL) {
    scanprevptr = scanptr;
    scanptr = scanptr->next;
  }
  if (scanptr == NULL) {
    DPRINT(COMM, DEBUG_ERROR, " NO available the message queue list\n");
    __OSAL_Mutex_UnLock(&g_MsqQHeader.hMutex);
    return -1;
  }

  if (scanprevptr == NULL) {
    g_MsqQHeader.start = scanptr->next;
  } else {
    scanprevptr->next = scanptr->next;
  }
  __OSAL_Mutex_UnLock(&g_MsqQHeader.hMutex);

  __OSAL_Mutex_Lock(&pList->hMutex);
  travptr = pList->first;

  while (travptr != NULL) {
    travprevptr = travptr;
    travptr = travptr->next;
    free(travprevptr->msgpacket);
    free(travprevptr);
  }
  __OSAL_Mutex_UnLock(&pList->hMutex);

  __OSAL_Event_Destroy(&pList->hEvent);
  __OSAL_Mutex_Destroy(&pList->hMutex);
  free(pList->queuename);
  free(pList);
  m_iMQhandle = MQ_INVALIDHANDLE;
  return 0;
}

/**
 * @brief         data ����
 * @remarks       message queue�� packet list�� packet structure�� attach.
 * @param         id          message id
 * @param         wParam      paramter (integer value)
 * @param         lParam      paramter (integer value)
 * @param         len         message data length
 * @param         msgdata     message data pointer
 * @param         type        message transfer type (unicast, multicast)
 * @return        ������ data byte
 */
int CbMessage::Send(int id,
                    int wParam,
                    int lParam,
                    int len,
                    void* msgData,
                    E_MSG_TYPE type) {
  MSG_PACKET packet;
  packet.id = id;
  packet.wParam = wParam;
  packet.lParam = lParam;
  packet.len = len;
  packet.msgdata = msgData;

  return Send(&packet, type);
}

/**
 * @brief         data ����
 * @remarks       message queue�� packet list�� packet structure�� attach.
 * @param         pPacket          data packet
 * @param         e_type      ���� type ( Unicast, multicast)
 * @return        ������ data byte
 */
int CbMessage::Send(PMSG_PACKET pPacket, E_MSG_TYPE e_type) {
  if (m_iMQhandle == MQ_INVALIDHANDLE) {
    DPRINT(COMM, DEBUG_ERROR, "CbMessage(0x%p)::Send-invalid message queue\n",
           this);
    return -1;
  }
  PMSGQ_HEAD pList = (PMSGQ_HEAD)m_iMQhandle;
  PMSGQ_LIST pNewMsg;
  int i_msgtosend = 1;
  __OSAL_Mutex_Lock(&pList->hMutex);
  if (e_type == MSG_BROADCAST) {
    i_msgtosend = pList->i_waitcount - pList->i_available;
  }
  while (i_msgtosend > 0) {
    pNewMsg = (PMSGQ_LIST)malloc(sizeof(MSGQ_LIST));
    CHECK_ALLOC(pNewMsg)

    pNewMsg->msgpacket = (MSG_PACKET*)malloc(sizeof(MSG_PACKET));
    CHECK_ALLOC(pNewMsg->msgpacket)
    if (pPacket->len > 0) {
      pNewMsg->msgpacket->msgdata = (void*)malloc(pPacket->len);
      CHECK_ALLOC(pNewMsg->msgpacket->msgdata);
      pNewMsg->msgpacket->len = pPacket->len;
      memcpy(pNewMsg->msgpacket->msgdata, pPacket->msgdata, pPacket->len);
    } else {
      pNewMsg->msgpacket->len = 0;
    }
    pNewMsg->msgpacket->id = pPacket->id;
    pNewMsg->msgpacket->wParam = pPacket->wParam;
    pNewMsg->msgpacket->lParam = pPacket->lParam;
    pNewMsg->next = NULL;

    pList->i_available++;

    if (e_type == MSG_BROADCAST) {
      pNewMsg->next = pList->first;
      pList->first = pNewMsg;
      if (pList->last == NULL)
        pList->last = pNewMsg;
    } else {
      if (pList->first == NULL)
        pList->first = pNewMsg;
      else
        pList->last->next = pNewMsg;
      pList->last = pNewMsg;
    }
    i_msgtosend--;
    __OSAL_Event_Send(&pList->hEvent);
  }
  __OSAL_Mutex_UnLock(&pList->hMutex);

  return pPacket->len;
}

/**
 * @brief         receive message.
 * @remarks       �ڽ��� message queue���� data�� ����
 * @param         pPacket    message packet structure
 * @param         i_msec     timeout
 * @return        ���ŵ� byte
 */
int CbMessage::Recv(PMSG_PACKET pPacket, int i_msec) {
  if (m_iMQhandle == MQ_INVALIDHANDLE) {
    DPRINT(COMM, DEBUG_ERROR, "CbMessage(0x%p)::Recv-invalid message queue\n",
           this);
    return -1;
  }

  PMSGQ_HEAD pList = (PMSGQ_HEAD)m_iMQhandle;
  PMSGQ_LIST returnmsg;
  PMSG_PACKET ptmpPacket;

  __OSAL_Mutex_Lock(&pList->hMutex);
  if (MQWTIME_WAIT_FOREVER == i_msec || i_msec > MQWTIME_WAIT_NO)
    pList->i_waitcount++;

  while (pList->i_available == 0) {
    if (MQWTIME_WAIT_NO == i_msec) {
      __OSAL_Mutex_UnLock(&pList->hMutex);
      return -1;
    } else {
      if (MQWTIME_WAIT_FOREVER == i_msec) {
        __OSAL_Event_Wait(&pList->hMutex, &pList->hEvent, -1);
      } else {
        if (__OSAL_Event_Wait(&pList->hMutex, &pList->hEvent, i_msec) == 0) {
          // return 0 means timeout
          pList->i_waitcount--;
          __OSAL_Mutex_UnLock(&pList->hMutex);
          return -1;
        }
      }
    }
  }

  pList->i_available--;
  returnmsg = pList->first;

  pList->first = returnmsg->next;
  if (pList->first == NULL)
    pList->last = NULL;

  if (-1 == i_msec || i_msec > 0)
    pList->i_waitcount--;

  ptmpPacket = returnmsg->msgpacket;

  if (ptmpPacket->len > 0) {
    pPacket->msgdata = (void*)malloc(ptmpPacket->len + 1);
    CHECK_ALLOC(pPacket->msgdata);
    memset(pPacket->msgdata, 0, ptmpPacket->len + 1);
    memcpy(pPacket->msgdata, ptmpPacket->msgdata, ptmpPacket->len);
    pPacket->len = ptmpPacket->len;
    free(ptmpPacket->msgdata);
  } else {
    pPacket->len = 0;
  }
  pPacket->id = ptmpPacket->id;
  pPacket->wParam = ptmpPacket->wParam;
  pPacket->lParam = ptmpPacket->lParam;
  free(ptmpPacket);
  free(returnmsg);

  __OSAL_Mutex_UnLock(&pList->hMutex);

  return pPacket->len;
}

/**
 * @brief         message queue pointerȹ��.
 * @remarks       �̸����� message queue�� pointer ȹ��
 * @param         name    	message queue name
 * @return        message queue base class pointer
 */
pMsgHandle mmBase::GetThreadMsgInterface(const char* name) {
  PMSGQ_HEAD pMsgQHead = g_MsqQHeader.start;
  while (pMsgQHead != NULL && (strcmp(pMsgQHead->queuename, name))) {
    pMsgQHead = pMsgQHead->next;
  }
  if (pMsgQHead != NULL) {
    return (pMsgHandle)pMsgQHead->pThreadmsgIF;
  }
  return NULL;
}
