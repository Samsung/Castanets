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

#include "Dispatcher.h"
#include "bMessage.h"
#include "bThread.h"

using namespace mmBase;

static CbDispatcher::subscribeObjDB_t g_SubscribeDB = {
    __OSAL_Mutex_Create(true), NULL};

bool CbDispatcher::Initialize() {
  if (!CbThread::ISRunning()) {
    StartMainLoop(NULL);
    return true;
  } else {
    DPRINT(COMM, DEBUG_WARN, "Dispatcher Already Started\n");
    return false;
  }
}

bool CbDispatcher::DeInitialize() {
  if (CbThread::ISRunning())
    StopMainLoop();
  return true;
}
/**
 * @brief         Dispatcher MainLoop
 * @remarks       Dispatcher MainLoop
 * @param         args    thread argument
 */

void CbDispatcher::MainLoop(void* args) {
  MSG_PACKET Packet;
  pFCB lpFunc;
  while (m_bRun) {
    CbDispatcher::subscribeObj_t* psObj = g_SubscribeDB.start;
    while (psObj != NULL) {
      CbMessage* pObj = (CbMessage*)psObj->pObj;
      if (pObj->Recv(&Packet, 1) >= 0) {
        __OSAL_Mutex_Lock(&g_SubscribeDB.hMutex);
        CbDispatcher::subscribeUnit_t* pSubscriber;
        int nCount = psObj->__LL_SU__.GetCount();

        for (int i = 0; i < nCount; i++) {
          pSubscriber = psObj->__LL_SU__.GetAt(i);
          if (pSubscriber->msgid == Packet.id) {
            lpFunc = pSubscriber->lpFunc;
            if (lpFunc) {
              (lpFunc)(Packet.wParam, Packet.lParam, Packet.msgdata, pObj);
            }
          }
        }
        if (Packet.len > 0) {
          if (Packet.msgdata != NULL)
            free(Packet.msgdata);
        }
        __OSAL_Mutex_UnLock(&g_SubscribeDB.hMutex);
        __OSAL_Sleep(100);
      }
      psObj = psObj->next;
    }
    __OSAL_Sleep(100);
  }
  return;
}

/**
 * @brief         message unsubscribe
 * @remarks       ��ϵ� callback�� ����
 * @param         msgid    	message id
 * @param         pClass    	caller class pointer
 * @return        ���� true, ���� flase
 */
BOOL CbDispatcher::UnSubscribe(int msgid,
                               void* pObj,
                               void (*pFCB)(int, int, void*, void*)) {
  subscribeObj_t* pScanPtr = g_SubscribeDB.start;
  subscribeObj_t* pScanPrevPtr = NULL;

  while (pScanPtr != NULL && pScanPtr->pObj != pObj) {
    pScanPrevPtr = pScanPtr;
    pScanPtr = pScanPtr->next;
  }

  if (pScanPtr != NULL) {
    __OSAL_Mutex_Lock(&g_SubscribeDB.hMutex);
    subscribeUnit_t* pSubscriber;
    int nCount = pScanPtr->__LL_SU__.GetCount();
    if (nCount > 1) {
      for (int i = 0; i < nCount; i++) {
        pSubscriber = pScanPtr->__LL_SU__.GetAt(i);
        if ((pSubscriber->msgid == msgid) && (pSubscriber->lpFunc == pFCB)) {
          pScanPtr->__LL_SU__.DelAt(i);
          break;
        }
      }
    } else  // callback list�� �����.
    {
      if (pScanPrevPtr == NULL) {
        g_SubscribeDB.start = pScanPtr->next;
      } else {
        pScanPrevPtr->next = pScanPtr->next;
      }
      pScanPtr->__LL_SU__.RemoveAll();
      free(pScanPtr);
    }
    __OSAL_Mutex_UnLock(&g_SubscribeDB.hMutex);
  } else {
    DPRINT(GLOB, DEBUG_ERROR, "Message is not registered!!n");
  }
  return true;
}

/**
 * @brief         message subscribe
 * @remarks       Ư�� message ���Ž� ȣ��� callback ���
 * @param         msgid    		message id
 * @param         pObj    	caller class pointer
 * @param         pFCB    		callback function pointer
 * @return        ���� true, ���� flase
 */
BOOL CbDispatcher::Subscribe(int msgid,
                             void* pObj,
                             void (*pFCB)(int, int, void*, void*)) {
  subscribeObj_t* pNewObject = g_SubscribeDB.start;
  while (pNewObject != NULL && pNewObject->pObj != pObj) {
    pNewObject = pNewObject->next;
  }
  if (pNewObject != NULL) {
    CbDispatcher::subscribeUnit_t* pSubscriber =
        new CbDispatcher::subscribeUnit_t;
    pSubscriber->msgid = msgid;
    pSubscriber->lpFunc = pFCB;
    __OSAL_Mutex_Lock(&g_SubscribeDB.hMutex);
    pNewObject->__LL_SU__.AddTail(pSubscriber);
    __OSAL_Mutex_UnLock(&g_SubscribeDB.hMutex);
  } else {
    pNewObject = new subscribeObj_t;
    pNewObject->pObj = pObj;
    CbDispatcher::subscribeUnit_t* pSubscriber =
        new CbDispatcher::subscribeUnit_t;
    if (pSubscriber == NULL) {
      DPRINT(COMM, DEBUG_ERROR,
             "Err*** Fail to Allocation Callback Info list\n");
      return false;
    }
    pSubscriber->msgid = msgid;
    pSubscriber->lpFunc = pFCB;
    __OSAL_Mutex_Lock(&g_SubscribeDB.hMutex);
    pNewObject->__LL_SU__.AddTail(pSubscriber);
    __OSAL_Mutex_UnLock(&g_SubscribeDB.hMutex);
    pNewObject->first = NULL;
    pNewObject->last = NULL;
    pNewObject->next = g_SubscribeDB.start;
    g_SubscribeDB.start = pNewObject;
  }

  return true;
}
