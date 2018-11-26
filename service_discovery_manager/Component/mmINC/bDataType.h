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

#ifndef __INCLUDE_COMMON_DATATYPE_H__
#define __INCLUDE_COMMON_DATATYPE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************************************************************
  Data type redefine
********************************************************************/
/*
#ifndef BOOLEAN
#define	BOOLEAN		boolean
#endif
*/

#ifndef BOOL
#ifdef WIN32
#define BOOL int
#else
#define BOOL bool  /// For nke94
#endif
#endif

#ifndef CHAR
#define CHAR char
#endif

#ifndef PCHAR
#define PCHAR char*
#endif

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef INT8
#define INT8 char
#endif

#ifndef UINT16
#define UINT16 unsigned short
#endif

#ifndef INT16
#define INT16 signed short
#endif

#ifndef UINT32
#define UINT32 unsigned int
#endif

#ifndef INT32
#define INT32 int
#endif

#ifndef LONG32
#define LONG32 signed long
#endif

#ifndef ULONG32
#define ULONG32 unsigned long
#endif

#ifndef INT32
#define INT32 int
#endif

#ifndef UINT64
#ifdef WIN32
#define UINT64 unsigned __int64
#elif defined(LINUX)
#define UINT64 unsigned long long
#endif
#endif

#ifndef INT64
#ifdef WIN32
#define INT64 unsigned __int64
#elif defined(LINUX)
#define INT64 signed long long
#endif

#endif

#ifndef MCHAR
#define MCHAR wchar_t
#endif

#ifndef FP32
#define FP32 float
#endif

#ifndef UFP32
#define UFP32 unsigned float
#endif

#ifndef FP64
#define FP64 double
#endif

#ifndef UFP64
#define UFP64 unsigned double
#endif

#ifndef PVOID
#define PVOID void*
#endif

#ifndef VOID
#define VOID void
#endif

#define PFHANDLE FILE*

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define ON TRUE
#define OFF FALSE

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef interface
#define interface class
#endif

#endif
