/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <dlog.h>

#include "XW_Extension.h"
#include "XW_Extension_EntryPoints.h"
#include "XW_Extension_Permissions.h"
#include "XW_Extension_Runtime.h"
#include "XW_Extension_SyncMessage.h"
#include "widget_api.h"

#define DLOG(msg) dlog_print(DLOG_ERROR, "WRT", msg)

XW_Extension g_xw_extension = 0;
const XW_CoreInterface* g_core = nullptr;
const XW_MessagingInterface* g_messaging = nullptr;
const XW_Internal_SyncMessagingInterface* g_sync_messaging = nullptr;
const XW_Internal_EntryPointsInterface* g_entry_points = nullptr;
const XW_Internal_RuntimeInterface* g_runtime = nullptr;

extern "C" int32_t XW_Initialize(XW_Extension extension,
                                 XW_GetInterface get_interface) {
  g_xw_extension = extension;
  g_core = reinterpret_cast<const XW_CoreInterface*>(
      get_interface(XW_CORE_INTERFACE));
  if (!g_core) {
    DLOG("Can't initialize extension: error getting Core interface.");
    return XW_ERROR;
  }

  g_messaging = reinterpret_cast<const XW_MessagingInterface*>(
      get_interface(XW_MESSAGING_INTERFACE));
  if (!g_messaging) {
    DLOG("Can't initialize extension: error getting Messaging interface.");
    return XW_ERROR;
  }

  g_sync_messaging =
      reinterpret_cast<const XW_Internal_SyncMessagingInterface*>(
          get_interface(XW_INTERNAL_SYNC_MESSAGING_INTERFACE));
  if (!g_sync_messaging) {
    DLOG("Can't initialize extension: error getting SyncMessaging interface.");
    return XW_ERROR;
  }

  g_entry_points = reinterpret_cast<const XW_Internal_EntryPointsInterface*>(
      get_interface(XW_INTERNAL_ENTRY_POINTS_INTERFACE));
  if (!g_entry_points) {
    DLOG("NOTE: Entry points interface not available in this version "
         "of Crosswalk, ignoring entry point data for extensions.");
    return XW_ERROR;
  }

  g_runtime = reinterpret_cast<const XW_Internal_RuntimeInterface*>(
      get_interface(XW_INTERNAL_RUNTIME_INTERFACE));
  if (!g_runtime) {
    DLOG("NOTE: runtime interface not available in this version "
         "of Crosswalk, ignoring runtime variables for extensions.");
    return XW_ERROR;
  }

  g_core->SetExtensionName(g_xw_extension, "Widget");
  const char* entry_points[] = {"widget", nullptr};
  g_entry_points->SetExtraJSEntryPoints(g_xw_extension, entry_points);
  g_core->SetJavaScriptAPI(g_xw_extension, kSource_widget_api);

  return XW_OK;
}
