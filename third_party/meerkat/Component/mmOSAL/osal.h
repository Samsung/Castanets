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

#ifndef __INCLUDE_OSAL_H__
#define __INCLUDE_OSAL_H__


#include "posixAPI.h"
#include "ioAPI.h"
#include "socketAPI.h"
#include "daemonAPI.h"
#include "timeAPI.h"

BOOL __OSAL_Initialize();
BOOL __OSAL_DeInitialize();

#endif
