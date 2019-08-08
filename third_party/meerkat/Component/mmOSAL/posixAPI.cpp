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

#include "posixAPI.h"
#include "bDataType.h"
#include "bGlobDef.h"
#include "Debugger.h"

#ifdef LINUX
#include <sys/time.h>
#include <unistd.h>
#endif

BOOL __OSAL_PosixAPI_Init() {
  return TRUE;
}

BOOL __OSAL_PosixAPI_DeInit() {
  return TRUE;
}

/**
 * @brief        mutex create
 * @remarks      mutex create os abstraction api
 * @return       OSAL_Mutex_Handle
 */
OSAL_Mutex_Handle __OSAL_Mutex_Create(BOOL recursive) {
#ifdef WIN32
  return CreateMutex(NULL, 0, NULL);
#elif defined(LINUX)
  pthread_mutex_t hMutex;
  if (recursive) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&hMutex, &attr);
  } else {
    pthread_mutex_init(&hMutex, NULL);
  }
  return hMutex;
#endif
}

/**
 * @brief        mutex destroy
 * @remarks      mutex destroy os abstraction api
 * @return       OSAL_Mutex_Handle
 */
OSAL_Posix_Return __OSAL_Mutex_Destroy(OSAL_Mutex_Handle* pMutex) {
#ifdef WIN32
  return CloseHandle(*pMutex);
#elif defined(LINUX)
  return pthread_mutex_destroy(pMutex);
#endif
}

/**
 * @brief        mutex lock
 * @remarks      mutex lock os abstraction api
 * @param        pMutex    	mutex pointer
 * @return       OSAL_Mutex_Handle
 */
OSAL_Posix_Return __OSAL_Mutex_Lock(OSAL_Mutex_Handle* pMutex) {
#ifdef WIN32
  return WaitForSingleObject(*pMutex, INFINITE);
#elif defined(LINUX)
  return pthread_mutex_lock(pMutex);
#endif
}

/**
 * @brief        mutex unlock
 * @remarks      mutex unlock os abstraction api
 * @param        pMutex    	mutex pointer
 * @return       OSAL_Mutex_Handle
 */
OSAL_Posix_Return __OSAL_Mutex_UnLock(OSAL_Mutex_Handle* pMutex) {
#ifdef WIN32
  return ReleaseMutex((HANDLE)*pMutex);
#elif defined(LINUX)
  return pthread_mutex_unlock(pMutex);
#endif
}

/**
 * @brief        event create
 * @remarks      event create os abstraction api
 * @return       OSAL_Event_Handle
 */
OSAL_Event_Handle __OSAL_Event_Create(void) {
#ifdef WIN32
  return CreateEvent(NULL, FALSE, FALSE, NULL);
#elif defined(LINUX)
  pthread_cond_t hEvent;
  pthread_cond_init(&hEvent, NULL);
  return hEvent;
#endif
}

/**
 * @brief        event destroy
 * @remarks      event destroy os abstraction api
 * @param        pEvent    	event pointer
 * @return       OSAL_Posix_Return
 */
OSAL_Posix_Return __OSAL_Event_Destroy(OSAL_Event_Handle* pEvent) {
#ifdef WIN32
  return CloseHandle(*pEvent);
#elif defined(LINUX)
  return pthread_cond_destroy(pEvent);
#endif
}

/**
 * @brief        event send
 * @remarks      event send os abstraction api
 * @param        pEvent    	event pointer
 * @return       OSAL_Posix_Return
 */
OSAL_Posix_Return __OSAL_Event_Send(OSAL_Event_Handle* pEvent) {
#ifdef WIN32
  return SetEvent(*pEvent);
#elif defined(LINUX)
  return pthread_cond_signal(pEvent);
#endif
}

/**
 * @brief        event wait
 * @remarks      event wait os abstraction api
 * @param        pMutex    	mutex handle pointer
 * @param        pEvent    	event handle pointer
 * @param        waitTime    	wait time value
 * @return       OSAL_Event_Status	 0-->Wait Timeout,
 *					 +1-->Get Signal,
 *					 -1-->Error
 */
OSAL_Event_Status __OSAL_Event_Wait(OSAL_Mutex_Handle* pMutex,
                                    OSAL_Event_Handle* pEvent,
                                    int waitTime) {
#ifdef WIN32
//  WaitForSingleObject(*pMutex, INFINITE);
  if (waitTime < 0) {
    WaitForSingleObject(*pEvent, INFINITE);
 //   ReleaseMutex(pMutex);
    return OSAL_EVENT_WAIT_GETSIG;
  } else {
    if (WaitForSingleObject(*pEvent, waitTime) == WAIT_TIMEOUT) {
//      ReleaseMutex(*pMutex);
      return OSAL_EVENT_WAIT_TIMEOUT;
    } else {
//      ReleaseMutex(*pMutex);
      return OSAL_EVENT_WAIT_GETSIG;
    }
  }
#elif defined(LINUX)
  if (waitTime < 0) {
    pthread_cond_wait(pEvent, pMutex);
    return OSAL_EVENT_WAIT_GETSIG;
  } else {
    struct timeval tv;
    struct timespec abstime;

    if (gettimeofday(&tv, NULL) != 0) {
      return OSAL_EVENT_WAIT_ERROR;
    }
    abstime.tv_sec = tv.tv_sec + waitTime / 1000;
    abstime.tv_nsec = (tv.tv_usec + waitTime % 1000) * 1000;
    if (pthread_cond_timedwait(pEvent, pMutex, &abstime))
      return OSAL_EVENT_WAIT_TIMEOUT;
    else
      return OSAL_EVENT_WAIT_GETSIG;
  }
#endif
}

/**
 * @brief        create thread
 * @remarks      create thread os abstraction api
 * @param        pStartRoutine    	thread entry function pointer
 * @param        pParam    		parameter pointer
 * @return       OSAL_Thread_Handle
 */
OSAL_Thread_Handle __OSAL_Create_Thread(void* pStartRoutine, void* pParam) {
#ifdef WIN32
  DWORD dwThreadId;
  HANDLE hThread;
  hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pStartRoutine, pParam,
                         CREATE_SUSPENDED, &dwThreadId);
  if (hThread == NULL) {
    DPRINT(GLOB, DEBUG_FATAL, "Thread Create Error!!!\n");
	return NULL;
    //		_ASSERT(0);
  }

  ResumeThread(hThread);
  return hThread;
#elif defined(LINUX)
  pthread_t hThread;
  int rc =
      pthread_create(&hThread, NULL, (void* (*)(void*))pStartRoutine, pParam);
  if (rc) {
    DPRINT(GLOB, DEBUG_FATAL, "Thread Create Error!!!\n");
	return 0;
    //		_ASSERT(0);
  }
  return hThread;
#endif
}

/**
 * @brief        join thread
 * @remarks      join thread os abstraction api
 * @param        hThread    	thread handle
 * @param        maxWait    	wait time
 * @return       OSAL_Posix_Return
 */
OSAL_Posix_Return __OSAL_Join_Thread(OSAL_Thread_Handle hThread, int maxWait) {
#ifdef WIN32
  DWORD dwRet = WaitForSingleObject(hThread, maxWait);
  if (dwRet == WAIT_TIMEOUT) {
    DPRINT(GLOB, DEBUG_FATAL, "Terminating stream main thread!!!\n");
    TerminateThread(hThread, 0);
  }
  CloseHandle(hThread);
  return 0;
#elif defined(LINUX)
  return pthread_join(hThread, NULL);
#endif
}

/**
 * @brief        sleep
 * @remarks      sleep os abstraction api
 * @param        mSec    	wait time
 */
void __OSAL_Sleep(int mSec) {
#ifdef WIN32
  Sleep(mSec);
#elif defined(LINUX)
  usleep(mSec * 1000);
#endif
}
