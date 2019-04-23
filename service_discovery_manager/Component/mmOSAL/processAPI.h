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

#ifndef __INCLUDE_OSAL_PROCESS_API_H__
#define __INCLUDE_OSAL_PROCESS_API_H__

#ifdef WIN32
#include <windows.h>
#elif defined(LINUX)

#endif

#ifdef WIN32
#define OSAL_PROCESS_ID HANDLE
#elif defined(LINUX)
#define OSAL_PROCESS_ID int
#endif

#include <sys/types.h>
#include <vector>

BOOL __OSAL_Create_Child_Process(std::vector<char*>& argument, 
                                 OSAL_PROCESS_ID* ppid, 
                                 OSAL_PROCESS_ID* ptid);

VOID __OSAL_Write_To_Pipe(char* std_in, int len);

VOID __OSAL_Read_From_Pipe(char* std_out, int* len);

#endif
