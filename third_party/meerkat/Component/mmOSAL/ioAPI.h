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

#ifndef __INCLUDE_IOAPI_H__
#define __INCLUDE_IOAPI_H__

#include "posixAPI.h"
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(LINUX)
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif
#include "bDataType.h"

#ifdef WIN32
#define OSAL_IO_Handle HANDLE
#define OSAL_IO_Return int
#else
#define OSAL_IO_Handle int
#define OSAL_IO_Return int
#endif

#define OSAL_IO_Error 0
#define OSAL_IO_Success 1

BOOL __OSAL_IOAPI_Init();
BOOL __OSAL_IOAPI_DeInit();

// return value ret<0:Error, ret>=0:success
OSAL_IO_Return __OSAL_IO_Open(/*[IN]*/ char* device,
                              /*[IN]*/ int opt,
                              OSAL_IO_Handle* pHandle);
OSAL_IO_Return __OSAL_IO_Read(/*[IN]*/ OSAL_IO_Handle handle,
                              /*[OUT]*/ char* buff,
                              /*[IN]*/ int toread,
                              /*[OUT]*/ int* preadbyte);

OSAL_IO_Return __OSAL_IO_Write(/*[IN]*/ OSAL_IO_Handle handle,
                               /*[IN]*/ char* buff,
                               /*[IN]*/ int towrite,
                               /*[OUT]*/ int* pwrittenbyte);

OSAL_IO_Return __OSAL_IO_Close(/*[IN]*/ OSAL_IO_Handle handle);

#endif
