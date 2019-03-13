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

/**
* @file      CiConfig.cpp
* @brief     print, debug, module id 설정
* @author   nangumg eun
* @date     2009/06/30
*/

#ifdef WIN32

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif

#include "Debugger.h"
#include "bGlobDef.h"
#include "bDataType.h"


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "posixAPI.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define DBG_LEVEL_STREAM "./DebugLevel"
#define DBG_FORMAT_STREAM "./DebugFormat"
#define DBG_FLAG_STREAM "./DebugFlag"

DEBUG_LEVEL g_iDebugLevel;
DEBUG_FORMAT g_fmtDebug;
int g_fDebugModeFlag;
BOOL g_bRunDaemon;

OSAL_Thread_Handle g_DebuggerHandle;

char szModulePrefix[][MODULE_PREFIX_MAX] = {"BLNK", "GLOB", "CMMN", "CONN"};

#ifdef WIN32
#if !defined(_UNICODE)
#define __bufferdprint _vsnprintf
#else
#define __bufferdprint _vsnprintf
#endif
#elif defined(LINUX)
#define __bufferdprint vsnprintf
#endif

void* debugLoop(void* args) {
  char input[64];
  char strNum[64];
  int num = 0;

  while (true) {
    DPRINT(GLOB, DEBUG_INFO, "START DEBUG MONITOR DAEMON\n");
    scanf("%s", input);

    if (!strcmp(input, "debug")) {
      for (;;) {
        RAW_PRINT("=====DEBUG MENU=====\n");
        RAW_PRINT("(0x1) Set Debug Level\n");
        RAW_PRINT("(0x2) Set Debug Format\n");
        RAW_PRINT("(0x3) Set Module Debug Flag\n");
        RAW_PRINT("(0x9) Exit.\n");
        RAW_PRINT("0x");
        scanf("%s", strNum);
        num = atoi(strNum);
        if (num == 9)
          break;
        else if (num == 1) {
          for (;;) {
            char strSel[32];
            int sel = 0;
            RAW_PRINT("==> Select Debug Level\n");
            RAW_PRINT("(0x1) Set Debug Level - Fatal\n");
            RAW_PRINT("(0x2) Set Debug Level - Error\n");
            RAW_PRINT("(0x3) Set Debug Level - Warning\n");
            RAW_PRINT("(0x4) Set Debug Level - Info\n");
            RAW_PRINT("(0x5) Set Debug Level - All\n");
            RAW_PRINT("(0x9) Exit.\n");
            RAW_PRINT("0x");
            scanf("%s", strSel);
            sel = atoi(strSel);

            if (sel == 9)
              break;
            else if (sel == 1) {
              SetDebugLevel(DEBUG_FATAL);
              RAW_PRINT("Set Debug Level - Fatal\n");
            } else if (sel == 2) {
              SetDebugLevel(DEBUG_ERROR);
              RAW_PRINT("Set Debug Level - Error\n");
            } else if (sel == 3) {
              SetDebugLevel(DEBUG_WARN);
              RAW_PRINT("Set Debug Level - Warning\n");
            } else if (sel == 4) {
              SetDebugLevel(DEBUG_INFO);
              RAW_PRINT("Set Debug Level - Info\n");
            } else if (sel == 5) {
              SetDebugLevel(DEBUG_ALL);
              RAW_PRINT("Set Debug Level - All\n");
            }
          }
        } else if (num == 2) {
          for (;;) {
            char strSel[32];
            int sel = 0;
            RAW_PRINT("==> Select Debug Format\n");
            RAW_PRINT("(0x1) Set Debug Format - Normal\n");
            RAW_PRINT("(0x2) Set Debug Format - Detail\n");
            RAW_PRINT("(0x9) Exit.\n");
            RAW_PRINT("0x");
            scanf("%s", strSel);
            sel = atoi(strSel);

            if (sel == 9)
              break;
            else if (sel == 1) {
              SetDebugFormat(DEBUG_NORMAL);
              RAW_PRINT("Set Debug Level - Normal\n");
            } else if (sel == 2) {
              SetDebugFormat(DEBUG_DETAIL);
              RAW_PRINT("Set Debug Level - Error\n");
            } else
              RAW_PRINT("You selected Invalid Number\n");
          }

        } else if (num == 3) {
          for (;;) {
            char strSel[32];
            int sel = 0;
            RAW_PRINT("==> Select Debug Module\n");
            for (int i = 0; i < (int)MODULE_ALL; i++) {
              RAW_PRINT("(0x%d) %s --", i + 1, szModulePrefix[i]);
              if (GetModuleDebugFlag((MODULE_ID)i))
                RAW_PRINT("[ON]\n");
              else
                RAW_PRINT("[OFF]\n");
            }
            RAW_PRINT("(0x9) Exit.\n");
            RAW_PRINT("0x");
            scanf("%s", strSel);
            sel = atoi(strSel);
            if (sel == 9) {
              break;
            } else {
              if ((sel > 0) && (sel < (int)MODULE_ALL)) {
                RAW_PRINT("==> Select Debug Option\n");
                RAW_PRINT("(0x1) ON\n");
                RAW_PRINT("(0x2) OFF\n");
                char strOnOff[32];
                int OnOff = 0;
                scanf("%s", strOnOff);
                OnOff = atoi(strOnOff);
                if (OnOff == 1) {
                  SetModuleDebugFlag((MODULE_ID)(sel - 1), true);
                } else if (OnOff == 2) {
                  SetModuleDebugFlag((MODULE_ID)(sel - 1), false);
                }
              } else {
                RAW_PRINT("InValid Module\n");
              }
            }
          }
        }
      }
    }
  }
  DPRINT(GLOB, DEBUG_INFO, "END DEBUG MONITOR DAEMON\n");

  return NULL;
}

void InitDebugInfo(BOOL bRunning) {
  DEBUG_LEVEL init_dbg_level = DEBUG_FATAL;
  DEBUG_FORMAT init_dbg_format = DEBUG_NORMAL; /*Normal Debug level*/
  int init_dbg_flag = 0;                       /*All Debug Disable*/

  FILE* fpl = fopen(DBG_LEVEL_STREAM, "r");

  if (fpl == NULL) {
    fpl = fopen(DBG_LEVEL_STREAM, "w");
    if (fpl) {
      fprintf(fpl, "%d", (int)init_dbg_level);
      fscanf(fpl, "%d", (int*)&g_iDebugLevel);
      fclose(fpl);
    } else {
      g_iDebugLevel = init_dbg_level;
    }
  } else {
    fscanf(fpl, "%d", (int*)&g_iDebugLevel);
    fclose(fpl);
  }
  FILE* fpf = fopen(DBG_FLAG_STREAM, "r");
  if (fpf == NULL) {
    fpf = fopen(DBG_FLAG_STREAM, "w");
    if (fpf) {
      fprintf(fpf, "%d", init_dbg_flag);
      fscanf(fpf, "%d", &g_fDebugModeFlag);
      fclose(fpf);
    } else {
      g_fDebugModeFlag = init_dbg_flag;
    }
  } else {
    fscanf(fpf, "%d", &g_fDebugModeFlag);
    fclose(fpf);
  }

  FILE* fpfmt = fopen(DBG_FORMAT_STREAM, "r");
  if (fpfmt == NULL) {
    fpfmt = fopen(DBG_FORMAT_STREAM, "w");
    if (fpf) {
      fprintf(fpfmt, "%d", (int)init_dbg_format);
      fscanf(fpfmt, "%d", (int*)&g_fmtDebug);
      fclose(fpfmt);
    } else {
      g_fmtDebug = init_dbg_format;
    }
  } else {
    fscanf(fpfmt, "%d", (int*)&g_fmtDebug);
    fclose(fpfmt);
  }
  g_bRunDaemon = bRunning;
  if (g_bRunDaemon)
    g_DebuggerHandle = __OSAL_Create_Thread((void*)debugLoop, NULL);
}

void CleanupDebugger() {
  if (g_bRunDaemon)
    __OSAL_Join_Thread(g_DebuggerHandle, 3000);
}

void SetDebugFormat(DEBUG_FORMAT format) {
  g_fmtDebug = format;
  FILE* fp = fopen(DBG_FORMAT_STREAM, "w");
  if (fp == NULL) {
    return;
  }
  fprintf(fp, "%d", g_fmtDebug);
  fclose(fp);
}

/**
 * @brief         SetDebugLevel
 * @remarks       check start variable
 */
void SetDebugLevel(DEBUG_LEVEL level) {
  g_iDebugLevel = level;
  FILE* fp = fopen(DBG_LEVEL_STREAM, "w");
  if (fp == NULL) {
    return;
  }
  fprintf(fp, "%d", g_iDebugLevel);
  fclose(fp);
}

/**
 * @brief        SetModuleDebugFlag
 * @remarks      SetModuleDebugFlag
 * @return       성공 true, 실패 flase
 */
bool SetModuleDebugFlag(MODULE_ID _id, bool bEnable) {
  if (_id == MODULE_ALL) {
    for (int i = 0; i < _id; i++) {
      if (bEnable)
        g_fDebugModeFlag = (g_fDebugModeFlag | (0x1 << i));
      else
        g_fDebugModeFlag = 0;
    }
  } else if (_id < MODULE_ALL && _id >= 0) {
    if (bEnable)
      g_fDebugModeFlag = (g_fDebugModeFlag | (0x1 << _id));
    else
      g_fDebugModeFlag = (g_fDebugModeFlag & (~(0x1 << _id)));
  } else {
    RAW_PRINT("[SetModuleDebugFlag] Unknown Module %d !!!\n", _id);
    return false;
  }

  FILE* fp = fopen(DBG_LEVEL_STREAM, "w");
  if (fp == NULL) {
    return false;
  }
  fprintf(fp, "%d", g_fDebugModeFlag);
  fclose(fp);

  return true;
}

/**
 * @brief         GetModuleDebugFlag
 * @remarks       GetModuleDebugFlag
 * @return        성공 true, 실패 flase
 */
bool GetModuleDebugFlag(MODULE_ID _id) {
  if (((g_fDebugModeFlag >> _id) & 0x1) == DEBUG_ON)
    return true;
  else
    return false;
}

void dbg_print(const char* file,
               int line,
               MODULE_ID id,
               DEBUG_LEVEL level,
               const char* fmt,
               ...) {
  if (((int)g_iDebugLevel) < ((int)level))
    return;

  if (!GetModuleDebugFlag(id))
    return;

  if (g_fmtDebug == DEBUG_DETAIL)
    fprintf(stderr, "[%s:%d]\n", file, line);

  if (id != BLNK)
    fprintf(stderr, "\t%s >> ", szModulePrefix[id]);

  char szBuffer[DEBUG_STR_MAX];
  int npos = 0;
  va_list args;
  va_start(args, fmt);
  npos += __bufferdprint(szBuffer + npos, DEBUG_STR_MAX, fmt, args);
  va_end(args);

  fprintf(stderr, "%s", static_cast<char*>(szBuffer));
}
