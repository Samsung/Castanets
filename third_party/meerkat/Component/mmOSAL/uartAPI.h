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

#ifndef __INCLUDE_OSAL_UART_API_H__
#define __INCLUDE_OSAL_UART_API_H__

#include "bDataType.h"

#ifdef WIN32
#define OSAL_Device_Handle int
#elif defined(LINUX)
#define OSAL_Device_Handle int
#endif

#ifdef WIN32

#elif defined(LINUX)
struct u_attribute {
  int baud;
  int vtime;
  int vmin;
  int flag;
};
#endif

BOOL __OSAL_Uart_Init();
BOOL __OSAL_Uart_DeInit();

OSAL_Device_Handle __OSAL_Uart_Open(const char* node);
void __OSAL_Uart_Close(OSAL_Device_Handle desc);
int __OSAL_Uart_Write(OSAL_Device_Handle hDev, const char* buff, int len);
int __OSAL_Uart_Read(OSAL_Device_Handle hDev, char* buff, int max_read);
int __OSAL_Uart_Set(OSAL_Device_Handle hDev, u_attribute attr);

#endif
