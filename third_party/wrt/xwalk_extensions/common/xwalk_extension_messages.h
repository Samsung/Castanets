#ifndef XWALK_EXTENSIONS_COMMON_XWALK_EXTENSIONS_MESSAGES_H_
#define XWALK_EXTENSIONS_COMMON_XWALK_EXTENSIONS_MESSAGES_H_

#include "ipc/ipc_message_macros.h"
#include "private/ewk_wrt_private.h"

#define IPC_MESSAGE_START ExtensionMsgStart

IPC_STRUCT_TRAITS_BEGIN(Ewk_Wrt_Message_Data)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(reference_id)
IPC_STRUCT_TRAITS_END()

IPC_MESSAGE_ROUTED1(XWalkExtensionHostMsg_Message,
                    Ewk_Wrt_Message_Data /* data */)

IPC_SYNC_MESSAGE_ROUTED1_1(XWalkExtensionHostMsg_Message_Sync,
                           Ewk_Wrt_Message_Data /* data */,
                           std::string /* result */)

#endif  // XWALK_EXTENSIONS_COMMON_XWALK_EXTENSIONS_MESSAGES_H_
