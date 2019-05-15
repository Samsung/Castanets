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

#ifndef __INCLUDE_COMMON_OBJCONFIG_H__
#define __INCLUDE_COMMON_OBJCONFIG_H__


#include "bDataType.h"

#ifdef LINUX
#include <assert.h>
#endif

#if defined(ANDROID)
#include <android/log.h>
#endif

#include "bDataType.h"

#define DEBUG_STR_MAX 512
#define PREFIX_STR_MAX 4

#define MODULE_PREFIX_MAX 7

#define DEBUG_ON TRUE
#define DEBUG_OFF FALSE

enum DEBUG_FORMAT { DEBUG_NORMAL = 0, DEBUG_DETAIL, DEBUG_FORMAT_MAX };

enum DEBUG_LEVEL {
  DEBUG_FATAL = 0,
  DEBUG_ERROR,
  DEBUG_WARN,
  DEBUG_INFO,
  DEBUG_ALL,
  DEBUG_LEVEL_MAX
};

enum MODULE_ID { BLNK = 0, GLOB, COMM, CONN, MODULE_ALL };
#if defined(WIN32)
#define DPRINT(prefix, level, f_, ...) \
  dbg_print(__FILE__, __LINE__, prefix, level, (f_), ##__VA_ARGS__)
#elif defined(ANDROID)
#define DPRINT(prefix, level, fmt, ...) \
  __android_log_print(ANDROID_LOG_DEBUG, #prefix, fmt, ##__VA_ARGS__)
#else
#define DPRINT(prefix, level, str...) \
  dbg_print(__FILE__, __LINE__, prefix, level, ##str)
#endif

#if defined(ANDROID)
#define RAW_PRINT(fmt, ...) \
__android_log_print(ANDROID_LOG_INFO, "SERVICE-DISCOVERY", fmt, ##__VA_ARGS__)
#else
#define RAW_PRINT printf
#endif


#ifndef __ASSERT
#define __ASSERT(expr)                                                \
  {                                                                   \
    if (!(expr)) {                                                    \
      RAW_PRINT("Intentional abnormal termination.\n");               \
      RAW_PRINT("Use a debugger to keep track of the this point.\n"); \
      RAW_PRINT("Assertion : %s failed, (%s, %d)\n", #expr, __FILE__, \
                __LINE__);                                            \
      abort();                                                        \
    }                                                                 \
  }
#endif

#define CHECK_ALLOC(x)                                 \
  if (!x) {                                            \
    DPRINT(GLOB, DEBUG_FATAL, "Memory alloc Error\n"); \
    abort();                                           \
  }

#define SAFE_FREE(x) \
  if (x != NULL) {   \
    free(x);         \
    x = NULL;        \
  }

#define SAFE_DELETE(x) \
  if (x != NULL) {     \
    delete x;          \
    x = NULL;          \
  }

#define SAFE_DELETE_ARRAY(x) \
  if (x != NULL) {           \
    delete[] x;              \
    x = NULL;                \
  }

#define CREATE_ZERO_MEMORY(x, y) \
  if (x == NULL) {               \
    x = new char[y];             \
    memset(x, 0, y);             \
  }
void InitDebugInfo(BOOL bRunning = false, BOOL create_files = false);
void CleanupDebugger();

void SetDebugLevel(DEBUG_LEVEL _level);
void SetDebugFormat(DEBUG_FORMAT format);

bool SetModuleDebugFlag(MODULE_ID _id, bool bEnable);

bool GetModuleDebugFlag(MODULE_ID _id);
void Out_CallerMsg(MODULE_ID _id, const char* fmt, ...);
void Out_CallerInfo(MODULE_ID _id, const char* fmt, ...);

void dbg_print(const char* file,
               int line,
               MODULE_ID id,
               DEBUG_LEVEL level,
               const char* fmt,
               ...);
#endif
