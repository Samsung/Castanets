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

#ifndef __INCLUDE_COMMON_DISPATCHER_H__
#define __INCLUDE_COMMON_DISPATCHER_H__

#include "Debugger.h"
#include "bDataType.h"
#include "bGlobDef.h"

#include "TPL_SGT.h"
#include "bList.h"
#include "bThread.h"

namespace mmBase {

class CbDispatcher : public CbThread, public CSTI<CbDispatcher> {
 public:
  typedef void (*pFCB)(int wParam, int lParam, void* data, void* pParent);

  struct subscribeUnit_t {
    int msgid;
    pFCB lpFunc;
  };

  struct subscribeObj_t {
    CbList<mmBase::CbDispatcher::subscribeUnit_t> __LL_SU__;
    void* pObj;
    subscribeObj_t* next;
    subscribeObj_t* first;
    subscribeObj_t* last;
  };

  struct subscribeObjDB_t {
    OSAL_Mutex_Handle hMutex;
    subscribeObj_t* start;
  };

 public:
  /*
                  void* operator new(size_t size) {
                          DPRINT(COMM, DEBUG_FATAL,"Single tone class Allocation
     -- Invalid Assert\n");
                          __ASSERT(0);
                          return NULL;
                  }
  */

  virtual bool Initialize();
  virtual bool DeInitialize();
  virtual BOOL Subscribe(int msgid, void* pOjbect, pFCB lpFunc);
  virtual BOOL UnSubscribe(int msgid, void* pOjbect, pFCB lpFunc);
  /*
          public:
                  CbDispatcher();
                  CbDispatcher(const CHAR* name);
                  virtual ~CbDispatcher();
  */
 private:
  void MainLoop(void* args);
};
}  // namespace mmBase

#endif
