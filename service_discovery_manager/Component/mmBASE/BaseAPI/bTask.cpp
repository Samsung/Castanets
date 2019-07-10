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
#include "Debugger.h"
#include "bTask.h"

using namespace mmBase;
/**
 * @brief         생성자
 * @remarks       생성자
 */
CbTask::CbTask() {
  m_bHasMsgQueue = false;
  m_key = __OSAL_Mutex_Create();
}

/**
 * @brief         생성자
 * @remarks       생성자
 * @param         msgqname          message queue name
 */
CbTask::CbTask(const char* pszTaskName) : CbThread(pszTaskName) {
  m_bHasMsgQueue = false;
  m_key = __OSAL_Mutex_Create();
  if (pszTaskName != NULL) {
    if (CreateMsgQueue(pszTaskName) < 0) {
      DPRINT(COMM, DEBUG_ERROR,
             "[Warnning] Cannot Create Message Queue--Create Thread without "
             "Msg queue\n");
    } else {
      m_bHasMsgQueue = true;
    }
  }
}

/**
 * @brief         소멸자.
 * @remarks       소멸자.
 */
CbTask::~CbTask() {
  __OSAL_Mutex_Destroy(&m_key);
}

/**
 * @brief         Create.
 * @remarks       Create.
 * @return        성공 true, 실패 flase
 */
BOOL CbTask::Create() {
  CbThread::StartMainLoop(NULL);
  return TRUE;
}

/**
 * @brief         Destroy.
 * @remarks       Destroy.
 * @return        성공 true, 실패 flase
 */
BOOL CbTask::Destroy() {
  CbThread::StopMainLoop();
  DestroyMsgQueue();
  return TRUE;
}

/**
 * @brief         Send.
 * @remarks       자신의 message queue로 message 전송.
 * @param         id          message id
 * @param         wParam      paramter (integer value)
 * @param         lParam      paramter (integer value)
 * @param         len         message data length
 * @param         msgdata     message data pointer
 * @param         type        message transfer type (unicast, multicast)
 * @return        전송한 data byte
 */
int CbTask::Send(int id,
                 int wParam,
                 int lParam,
                 int len,
                 void* msgdata,
                 E_MSG_TYPE type) {
  MSG_PACKET packet;
  packet.id = id;
  packet.wParam = wParam;
  packet.lParam = lParam;
  packet.len = len;
  packet.msgdata = msgdata;
  return Send(&packet, type);
}

/**
 * @brief         Send.
 * @remarks       자신의 message queue로 message 전송.
 * @param         pPacket     message packet structure
 * @param         type        message transfer type (unicast, multicast)
 * @return        전송한 data byte
 */
int CbTask::Send(PMSG_PACKET pPacket, E_MSG_TYPE type) {
  return ((CbMessage*)this)->Send(pPacket, type);
}

/**
 * @brief         Send.
 * @remarks       지정된 message queue로 message 전송.
 * @param         pmsgQH      message queue pointer
 * @param         id          message id
 * @param         wParam      paramter (integer value)
 * @param         lParam      paramter (integer value)
 * @param         len         message data length
 * @param         msgdata     message data pointer
 * @param         type        message transfer type (unicast, multicast)
 * @return        전송한 data byte
 */
int CbTask::Send(pMsgHandle pmsgQH,
                 int id,
                 int wParam,
                 int lParam,
                 int len,
                 void* msgdata,
                 E_MSG_TYPE type) {
  MSG_PACKET packet;
  packet.id = id;
  packet.wParam = wParam;
  packet.lParam = lParam;
  packet.len = len;
  packet.msgdata = msgdata;
  return Send(pmsgQH, &packet, type);
}

/**
 * @brief         Send.
 * @remarks       지정된 message queue로 message 전송.
 * @param         pmsgQH      message queue pointer
 * @param         pPacket     message packet structure
 * @param         type        message transfer type (unicast, multicast)
 * @return        전송한 data byte
 */
int CbTask::Send(pMsgHandle pmsgQH, PMSG_PACKET pPacket, E_MSG_TYPE type) {
  return pmsgQH->Send(pPacket, type);
}

/**
 * @brief         receive message.
 * @remarks       자신의 message queue에서 data를 수신
 * @param         pPacket    message packet structure
 * @param         i_msec     timeout
 * @return        수신된 byte
 */
int CbTask::Recv(PMSG_PACKET pPacket, int i_msec) {
  return CbMessage::Recv(pPacket, i_msec);
}

void CbTask::CheckEvent() {
  pfCB lpFunc;
  MSG_PACKET Packet;

  if (Recv(&Packet, 100) >= 0) {
    __OSAL_Mutex_Lock(&m_key);
    int nCount = m_eventDB.GetCount();
    for (int i = 0; i < nCount; i++) {
      t_eventFromat* peventForamt = m_eventDB.GetAt(i);

      if (peventForamt->iid == Packet.id) {
        lpFunc = peventForamt->lpFunc;
        if (lpFunc) {
          (lpFunc)(Packet.wParam, Packet.lParam, Packet.msgdata, (void*)this);
        }
      }
    }
    if (Packet.len > 0) {
      if (Packet.msgdata != NULL)
        free(Packet.msgdata);
    }
    __OSAL_Mutex_UnLock(&m_key);
  }
}

BOOL CbTask::Subscribe(int msgid, pfCB lpFunc) {
  __OSAL_Mutex_Lock(&m_key);
  int nCount = m_eventDB.GetCount();

  for (int i = 0; i < nCount; i++) {
    t_eventFromat* peventForamt = m_eventDB.GetAt(i);
    if (peventForamt == NULL) {
      __OSAL_Mutex_UnLock(&m_key);
      __ASSERT(0);
    }
    if ((peventForamt->iid == msgid) && (peventForamt->lpFunc == lpFunc)) {
      DPRINT(COMM, DEBUG_ERROR, "Event (%d) is Already Subsribed at Task(%s)\n",
             msgid, CbThread::m_szThreadName);
      __OSAL_Mutex_UnLock(&m_key);
      return FALSE;
    }
  }

  t_eventFromat* pNewEvent = new t_eventFromat;
  pNewEvent->iid = msgid;
  pNewEvent->lpFunc = lpFunc;

  m_eventDB.AddTail(pNewEvent);
  __OSAL_Mutex_UnLock(&m_key);
  return TRUE;
}

BOOL CbTask::UnSubscribe(int msgid, pfCB lpFunc) {
  __OSAL_Mutex_Lock(&m_key);
  int nCount = m_eventDB.GetCount();
  for (int i = 0; i < nCount; i++) {
    t_eventFromat* peventForamt = m_eventDB.GetAt(i);
    if (peventForamt == NULL) {
      __OSAL_Mutex_UnLock(&m_key);
      __ASSERT(0);
    }
    if ((peventForamt->iid == msgid) && (peventForamt->lpFunc == lpFunc)) {
      m_eventDB.DelAt(i);
      __OSAL_Mutex_UnLock(&m_key);
      return TRUE;
    }
  }

  DPRINT(COMM, DEBUG_ERROR, "Event (%d) is Not Subsribed at Task(%s)\n", msgid,
         CbThread::m_szThreadName);
  __OSAL_Mutex_UnLock(&m_key);
  return FALSE;
}
