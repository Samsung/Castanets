// Copyright 2015-2016 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.

#include <vector>

#include "base/values.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc_message_start_ewk.h"
#include "private/ewk_wrt_private.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START EwkMsgStart

IPC_STRUCT_TRAITS_BEGIN(Ewk_Wrt_Message_Data)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(reference_id)
IPC_STRUCT_TRAITS_END()

IPC_MESSAGE_CONTROL2(WrtMsg_ParseUrl,
                     int,            // result: request_id
                     GURL)           // result: url

IPC_MESSAGE_CONTROL2(WrtMsg_ParseUrlResponse,
                     int,            // result: request_id
                     GURL)           // result: url

IPC_MESSAGE_CONTROL1(WrtMsg_SendWrtMessage,
                     Ewk_Wrt_Message_Data /* data */)

IPC_SYNC_MESSAGE_ROUTED1_1(EwkHostMsg_WrtSyncMessage,
                           Ewk_Wrt_Message_Data /* data */,
                           std::string /*result*/)

IPC_MESSAGE_ROUTED1(EwkHostMsg_WrtMessage,
                    Ewk_Wrt_Message_Data /* data */)
