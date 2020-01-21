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

#ifndef __INCLUDE_SGT_H__
#define __INCLUDE_SGT_H__

#include <stdio.h>
#include <stdlib.h>
#include "Debugger.h"
#include "bDataType.h"
#include "bGlobDef.h"
#include "posixAPI.h"

template <typename Class>
class CSTI {
 private:
  static Class* m_pInstance;

 protected:
  CSTI() {
    __ASSERT(!m_pInstance);
    long long offset =
        (long long)(Class*)1 - (long long)(CSTI<Class>*)(Class*)1;
    m_pInstance = (Class*)((long long)this + offset);
  }

  ~CSTI() {
    __ASSERT(m_pInstance);
    m_pInstance = NULL;
  }

 public:
  static Class& getInstance() {
    if (m_pInstance == NULL) {
      static Class obj;
      m_pInstance = &obj;
    }
    return *m_pInstance;
  }

  static Class* getInstancePtr() {
    if (m_pInstance == NULL) {
      static Class obj;
      m_pInstance = &obj;
    }
    return m_pInstance;
  }

  static void releaseInstance() { m_pInstance = NULL; }
};

template <typename Class>
Class* CSTI<Class>::m_pInstance = NULL;
#endif
