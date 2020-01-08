/*
 * Copyright (C) 2014-2016 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SAMSUNG ELECTRONICS. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SAMSUNG ELECTRONICS. OR ITS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ewk_ipc_message_internal.h"

#include "common/render_messages_ewk.h"
#include "content/public/renderer/render_view.h"
// #include "eweb_context.h"
// #include "private/ewk_context_private.h"
#include "private/ewk_wrt_private.h"

// TODO(sns.park):
// The API description is unclear about who will delete it (via eina_stringshare_del()).
// Better to give clear description about the ownership/lifetime of the returned string.
// More Better is to replace Eina_Stringshare* with simple const char* (like webkit2),
// and give documentation saying "returned string is valid as long as |data| is valid"

const char *ewk_ipc_wrt_message_data_type_get(const Ewk_IPC_Wrt_Message_Data *data)
{
    if (!data)
        return nullptr;
    return data->GetType();
}

bool ewk_ipc_wrt_message_data_type_set(Ewk_IPC_Wrt_Message_Data *data, const char *type)
{
    if (!data)
        return false;
    data->SetType(type);
    return true;
}

const char *ewk_ipc_wrt_message_data_value_get(const Ewk_IPC_Wrt_Message_Data *data)
{
    if (!data)
        return nullptr;
    return data->GetValue();
}

bool ewk_ipc_wrt_message_data_value_set(Ewk_IPC_Wrt_Message_Data *data, const char *value)
{
    if (!data)
        return false;
    data->SetValue(value);
    return true;
}

const char *ewk_ipc_wrt_message_data_id_get(const Ewk_IPC_Wrt_Message_Data *data)
{
    if (!data)
        return nullptr;
    return data->GetId();
}

bool ewk_ipc_wrt_message_data_id_set(Ewk_IPC_Wrt_Message_Data *data, const char *id)
{
    if (!data)
        return false;
    data->SetId(id);
    return true;
}

const char *ewk_ipc_wrt_message_data_reference_id_get(const Ewk_IPC_Wrt_Message_Data *data)
{
    if (!data)
        return nullptr;
    return data->GetReferenceId();
}

bool ewk_ipc_wrt_message_data_reference_id_set(Ewk_IPC_Wrt_Message_Data *data, const char *reference_id)
{
    if (!data)
        return false;
    data->SetReferenceId(reference_id);
    return true;
}
/*
bool ewk_ipc_plugins_message_send(int routingId, const Ewk_IPC_Wrt_Message_Data* data)
{
    content::RenderView* rv = content::RenderView::FromRoutingID(routingId);
    if (!rv) {
      return false;
    }
    rv->Send(new EwkHostMsg_WrtMessage(rv->GetRoutingID(), *data));
    return true;
}

bool ewk_ipc_wrt_message_send(Ewk_Context* context, const Ewk_IPC_Wrt_Message_Data* data)
{
    EWebContext* web_context = context->GetImpl();
    if (!web_context) {
      return false;
    }
    web_context->SendWrtMessage(*data);
    return true;
}

bool ewk_ipc_plugins_sync_message_send(int routingId, Ewk_IPC_Wrt_Message_Data* data)
{
    content::RenderView* rv = content::RenderView::FromRoutingID(routingId);
    if (!rv) {
      return false;
    }

    rv->Send(new EwkHostMsg_WrtSyncMessage(routingId, *data, &data->value));
    return true;
}
*/

Ewk_IPC_Wrt_Message_Data *ewk_ipc_wrt_message_data_new()
{
    return new Ewk_IPC_Wrt_Message_Data();
}

void ewk_ipc_wrt_message_data_del(Ewk_IPC_Wrt_Message_Data *data)
{
    delete data;
}
