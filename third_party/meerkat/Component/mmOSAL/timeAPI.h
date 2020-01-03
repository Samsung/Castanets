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

#ifndef __INCLUDE_TIMEAPI_H__
#define __INCLUDE_TIMEAPI_H__

#include "bDataType.h"

#ifdef LINUX
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define OSAL_Time_Return INT32
#define OSAL_Time_Error -1
#define OSAL_Time_Success 0

BOOL __OSAL_TimeAPI_Init();
BOOL __OSAL_TimeAPI_DeInit();

// return value ret<0:Error, ret>=0:success
OSAL_Time_Return __OSAL_TIME_GetTimeMS(UINT64* ptimeval);
OSAL_Time_Return __OSAL_TIME_GetTimeS(UINT32* ptimeval);
OSAL_Time_Return __OSAL_TIME_GetTimeWait(UINT64 timeval);

#endif
