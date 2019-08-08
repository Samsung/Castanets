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

#ifndef __INCLUDE_COMPONENT_TYPE_H__
#define __INCLUDE_COMPONENT_TYPE_H__

#define DC_EVENT 0x00006000

#define DC_PLUGIN_UNAME "datacardhandler"

typedef struct component_guid_t {
  unsigned long d_Long;
  unsigned short d_Short1;
  unsigned short d_Short2;
  unsigned char d_Char[8];
} COMPONENT_GUID;

inline bool operator==(const COMPONENT_GUID& rIID1,
                       const COMPONENT_GUID& rIID2) {
  return (((unsigned long*)&rIID1)[0] == ((unsigned long*)&rIID2)[0] &&
          ((unsigned long*)&rIID1)[1] == ((unsigned long*)&rIID2)[1] &&
          ((unsigned long*)&rIID1)[2] == ((unsigned long*)&rIID2)[2] &&
          ((unsigned long*)&rIID1)[3] == ((unsigned long*)&rIID2)[3]);
}

inline bool operator!=(const COMPONENT_GUID& rIID1,
                       const COMPONENT_GUID& rIID2) {
  return !(rIID1 == rIID2);
}

typedef COMPONENT_GUID COMPONENT_IID;
typedef unsigned long COMPONENT_MID;

typedef void hLIbrary_t;
typedef void* LP_INSTANCE;

#ifdef __cplusplus
#define MM_PUBLIC extern "C"
#endif

enum E_COMPONENT_ERROR_CODE {
  CC_OK = 0x0,
  CC_BASE = 0xC000000,
  CC_ERROR_UNKNOWN,
  CC_NOT_IMPLEMENTED,
  CC_NOT_ENOUGH_MEMORY,
  CC_NULL_PARAMETER,
  CC_NOT_CREATED,
  CC_ALREADY_CREATED,
  CC_NO_INTERFACE,
  CC_INVALID_LIBRARY,
  CC_NO_INFO,
  CC_PROCESSING_ERROR
};

typedef signed long ccRESULT;

#endif
