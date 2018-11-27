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

#ifndef __INCLUDE_POSIXAPI_H__
#define __INCLUDE_POSIXAPI_H__

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(LINUX)
#include <pthread.h>
#endif
#include "bDataType.h"

#ifdef WIN32
#define OSAL_Mutex_Handle HANDLE
#define OSAL_Event_Handle HANDLE
#define OSAL_Thread_Handle HANDLE
#define OSAL_Posix_Return DWORD
#elif defined(LINUX)
#define OSAL_Mutex_Handle pthread_mutex_t
#define OSAL_Event_Handle pthread_cond_t
#define OSAL_Thread_Handle pthread_t
#define OSAL_Posix_Return int
#endif

enum OSAL_Event_Status {
  OSAL_EVENT_WAIT_TIMEOUT = 0,
  OSAL_EVENT_WAIT_GETSIG = 1,
  OSAL_EVENT_WAIT_ERROR = 2,
  OSAL_EVENT_WAIT_MAX
};

BOOL __OSAL_PosixAPI_Init();
BOOL __OSAL_PosixAPI_DeInit();

OSAL_Mutex_Handle __OSAL_Mutex_Create(void);
OSAL_Posix_Return __OSAL_Mutex_Destroy(OSAL_Mutex_Handle* pMutex);
OSAL_Posix_Return __OSAL_Mutex_Lock(OSAL_Mutex_Handle* pMutex);
OSAL_Posix_Return __OSAL_Mutex_UnLock(OSAL_Mutex_Handle* pMutex);

OSAL_Event_Handle __OSAL_Event_Create(void);
OSAL_Posix_Return __OSAL_Event_Destroy(OSAL_Event_Handle* pEvent);
OSAL_Posix_Return __OSAL_Event_Send(OSAL_Event_Handle* pEvent);

/*0-->Wait Timeout, 1-->Get Signal ,2-->Error */
OSAL_Event_Status __OSAL_Event_Wait(OSAL_Mutex_Handle* pMutex,
                                    OSAL_Event_Handle* pEvent,
                                    int waitTime);

OSAL_Thread_Handle __OSAL_Create_Thread(void* pStartRoutine, void* pParam);
OSAL_Posix_Return __OSAL_Join_Thread(OSAL_Thread_Handle hThread, int maxWait);

void __OSAL_Sleep(int mSec);
#endif
