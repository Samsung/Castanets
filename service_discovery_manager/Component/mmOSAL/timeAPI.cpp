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

#include "timeAPI.h"
#include "bGlobDef.h"
#include "Debugger.h"


BOOL __OSAL_TimeAPI_Init() {
  DPRINT(COMM, DEBUG_INFO, "[OSAL] Timer Initialize\n");
  return TRUE;
}
BOOL __OSAL_TimeAPI_DeInit() {
  DPRINT(COMM, DEBUG_INFO, "[OSAL] Timer DeInitialize\n");
  return TRUE;
}
/**
 * @brief        get ms time
 * @remarks      get ms time
 * @param        ptimeval    	64bit time value
 * @return       OSAL_Time_Return
 */
OSAL_Time_Return __OSAL_TIME_GetTimeMS(UINT64* ptimeval) {
#ifdef WIN32
  *ptimeval = GetTickCount64();
  return OSAL_Time_Success;
#elif defined(LINUX)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *ptimeval = ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
  return OSAL_Time_Success;
#endif
}

OSAL_Time_Return __OSAL_TIME_GetTimeS(UINT32* ptimeval) {
#ifdef WIN32
  *ptimeval = GetTickCount()/1000;
  return OSAL_Time_Success;
#elif defined(LINUX)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *ptimeval = tv.tv_sec;
  return OSAL_Time_Success;
#endif
}

/**
 * @brief        wait ms time
 * @remarks      wait ms time
 * @param        ptimeval    	64bit time value
 * @return       OSAL_Time_Return
 */
OSAL_Time_Return __OSAL_TIME_GetTimeWait(UINT64 timeval) {
#ifdef WIN32
  return OSAL_Time_Success;
#elif defined(LINUX)
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = timeval * 1000;

  if (tv.tv_usec >= 1000000) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  if (select(0, NULL, NULL, NULL, &tv) < 0)
    return OSAL_Time_Error;
  return OSAL_Time_Success;
#endif
}
