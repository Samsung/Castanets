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

#include "NetTunProc.h"
#include "timeAPI.h"
#include "StunClient.h"

#include "netUtil.h"

using namespace mmBase;
using namespace mmProto;

#define MAX_STUN_MSG_BUFF 512

CNetTunProc* CNetTunProc::m_hNetTunProc = NULL;

CNetTunProc::CNetTunProc(const char* pszTaskName,
                         char* server_ip,
                         unsigned short tun_port,
                         int read_once,
                         int time_unit,
                         int bind_period,
                         int retry_count)
    : CbTask(pszTaskName) {
  t_CreateEvent();

  m_args.tun_port = tun_port;
  m_args.read_once = read_once;
  m_args.time_unit = time_unit;
  m_args.bind_period = bind_period;
  m_args.retry_count = retry_count;

  m_device_address.source_address = 0;
  m_device_address.source_port = m_args.tun_port;
  m_device_address.mapped_address = 0;
  m_device_address.mapped_port = 0;

  m_target_address.source_address = 0;
  m_target_address.source_port = 0;
  m_target_address.mapped_address = 0;
  m_target_address.mapped_port = 0;

  m_hNetTunProc = this;

  m_hasTarget = false;

  memset(m_args.server_ip, 0, 16);
  strcpy(m_args.server_ip, server_ip);
}

CNetTunProc::~CNetTunProc() {}

BOOL CNetTunProc::Create() {
  CbTask::Create();
  m_pTableHandler = new CRouteTable("localroute");
  m_pTableHandler->RouteTableCheckerStart();

  m_pRemoteServer = new CRmtServer("remoteserver");
  m_pRemoteServer->RemoteServerStart(OnRemoteMessage, m_args.tun_port,
                                     m_args.read_once);

  if (t_ProcessDHCP()) {
    m_pTunServer = new CTunServer("tunserver");

    char* pszTunAddr = U::CONV(m_device_address.source_address);
    m_pTunServer->TunnelingStart(OnLocalMessage, pszTunAddr, m_args.read_once);
    SAFE_DELETE(pszTunAddr);
  }

  return TRUE;
}

BOOL CNetTunProc::Destroy() {
  m_pTableHandler->RouteTableCheckerStop();
  m_pRemoteServer->RemoteServerStop();
  m_pTunServer->TunnelingStop();

  return CbTask::Destroy();
}

BOOL CNetTunProc::ProcessRemotePacket(
    char* sender_addr,
    int sender_port,
    CStunClient::STUN_MSG_TYPE Type,
    mmBase::CbList<CStunClient::stun_msg_attr>* attrList) {
  // processing remote packet
  if (Type == CStunClient::MAPQUERY_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [MAPQUERY_RESPONSE]--\n");
    CRouteTable::mapTable* pTableInfo = new CRouteTable::mapTable;

    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::MAPPED_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        pTableInfo->mapped_address = one_address.Address;
        pTableInfo->mapped_port = one_address.port;
      } else if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        pTableInfo->source_address = one_address.Address;
        pTableInfo->source_port = one_address.port;
      } else if (pattr->type == CStunClient::CHANGED_ADDRESS) {
        //
      }
    }

    pTableInfo->type = CRouteTable::CONN_NOT_ESTABLISHED;
    pTableInfo->state = CRouteTable::UNLOCK_TURN_CHANNEL;

    m_pTableHandler->AddPath(pTableInfo);

    DPRINT(COMM, DEBUG_INFO, "Add Table =>\n");
    U::SHOW_TABLE(pTableInfo);
    DPRINT(COMM, DEBUG_INFO, "GET [MAPQUERY_RESPONSE]++\n\n");

    __OSAL_Event_Send(&m_sigQuery.hEvent);

  } else if (Type == CStunClient::DHCP_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [DHCP_RESPONSE]--\n");
    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        m_device_address.source_address = one_address.Address;
        m_device_address.source_port = one_address.port;
        DPRINT(COMM, DEBUG_INFO, "GetDHCP =>\n");
        U::SHOW_ADDR("SOURCE", m_device_address.source_address,
                     m_device_address.source_port);
        DPRINT(COMM, DEBUG_INFO, "<=GetDHCP\n");
        break;
      }
    }
    DPRINT(COMM, DEBUG_INFO, "GET [DHCP_RESPONSE]++\n\n");
    __OSAL_Event_Send(&m_sigInit.hEvent);
  } else if (Type == CStunClient::BINDING_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [BINDING_RESPONSE]--\n");
    CRouteTable::mapTable map;
    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::MAPPED_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);

        map.mapped_address = one_address.Address;
        map.mapped_port = one_address.port;
        U::SHOW_ADDR("MAPPED", map.mapped_address, map.mapped_port);
      } else if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        map.source_address = one_address.Address;
        map.source_port = one_address.port;
        U::SHOW_ADDR("SOURCE", map.source_address, map.source_port);
      }
    }
    m_device_address.mapped_address = map.mapped_address;
    m_device_address.mapped_port = map.mapped_port;
    U::SHOW_ADDR("SOURCE", m_device_address.source_address,
                 m_device_address.source_port);
    U::SHOW_ADDR("MAPPED", m_device_address.mapped_address,
                 m_device_address.mapped_port);
    DPRINT(COMM, DEBUG_INFO, "GET [BINDING_RESPONSE]++\n\n");
    __OSAL_Event_Send(&m_sigAlive.hEvent);
  } else if (Type == CStunClient::TRIAL_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [TRIAL_RESPONSE]--\n");
    CRouteTable::mapTable map;
    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::MAPPED_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        map.mapped_address = one_address.Address;
        map.mapped_port = one_address.port;
      } else if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        map.source_address = one_address.Address;
        map.source_port = one_address.port;
      } else if (pattr->type == CStunClient::CHANGED_ADDRESS) {
        //
      }
    }
    /*
    if(m_pTableHandler->QueryTable(pTableInfo->source_address,pTableInfo->source_port,CRouteTable::QUERY_BY_SOURCEADDR)==NULL)
    {
            m_pTableHandler->AddPath(pTableInfo);
    }
    else
            SAFE_DELETE(pTableInfo);
    */
    m_pTableHandler->SetConnType(map.source_address, map.source_port,
                                 CRouteTable::DIRECT_STUN_CONN);
    U::SHOW_TABLE(&map);
    DPRINT(COMM, DEBUG_INFO, "GET [TRIAL_RESPONSE]++\n\n");
    __OSAL_Event_Send(&m_sigTrial.hEvent);

  } else if (Type == CStunClient::TURNALLOC_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [TURNALLOC_RESPONSE]--\n");
    CRouteTable::mapTable* pTableInfo = new CRouteTable::mapTable;
    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::MAPPED_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        pTableInfo->relay_address = one_address.Address;
        pTableInfo->relay_port = one_address.port;
        pTableInfo->mapped_address = 0;
        pTableInfo->mapped_port = 0;
      } else if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        pTableInfo->source_address = one_address.Address;
        pTableInfo->source_port = one_address.port;
      } else if (pattr->type == CStunClient::CHANGED_ADDRESS) {
        //
      }
    }
    pTableInfo->type = CRouteTable::RELAYED_TURN_CONN;
    pTableInfo->state = CRouteTable::LOCKED_TRUN_CHANNEL;
    m_pTableHandler->AddPath(pTableInfo);
    U::SHOW_TABLE(pTableInfo);
    DPRINT(COMM, DEBUG_INFO, "GET [TURNALLOC_RESPONSE]++\n\n");
    __OSAL_Event_Send(&m_sigTurnAlloc.hEvent);
  } else if (Type == CStunClient::TRIAL_REQUEST) {
    DPRINT(COMM, DEBUG_INFO, "GET [TRIAL_REQUEST]--\n");
    char response_buf[MAX_STUN_MSG_BUFF];
    memset(response_buf, 0, MAX_STUN_MSG_BUFF);

    CRouteTable::mapTable* pmap = new CRouteTable::mapTable;
    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::MAPPED_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);

        pmap->mapped_address = one_address.Address;
        pmap->mapped_port = one_address.port;

      } else if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);

        pmap->source_address = one_address.Address;
        pmap->source_port = one_address.port;
      }
    }
    int msg_len = CStunClient::bpRequest(
        response_buf, CStunClient::TRIAL_RESPONSE,
        m_device_address.source_address, m_device_address.source_port,
        m_device_address.mapped_address, m_device_address.mapped_port);
    U::SHOW_TABLE(pmap);
    char* szDestination = U::CONV(pmap->mapped_address);
    DPRINT(COMM, DEBUG_INFO, "SEND TRIAL RESPONSE (%s:%d)\n", szDestination,
           pmap->mapped_port);
    m_pRemoteServer->DataSend(sender_addr, response_buf, msg_len, sender_port);
    SAFE_DELETE(szDestination);
    /*
                    pmap->type=CRouteTable::DIRECT_STUN_CONN;
                    pmap->state=CRouteTable::UNLOCK_TURN_CHANNEL;
                    m_pTableHandler->AddPath(pmap);
    */
    DPRINT(COMM, DEBUG_INFO, "GET [TRIAL_REQUEST]++\n\n");
  } else if (Type == CStunClient::TARGETB_RESPONSE ||
             Type == CStunClient::TARGETR_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [TARGET_RESPONSE]--\n");
    CRouteTable::mapTable map;
    int len = attrList->GetCount();
    for (int i = 0; i < len; i++) {
      CStunClient::stun_msg_attr* pattr = attrList->GetAt(i);
      if (pattr->type == CStunClient::MAPPED_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        m_target_address.mapped_address = one_address.Address;
        m_target_address.mapped_port = one_address.port;
        if (m_device_address.mapped_address == one_address.Address) {
          m_hasTarget = false;
        } else {
          m_hasTarget = true;
        }
        U::SHOW_ADDR("TARGET MAPPED", one_address.Address, one_address.port);
      } else if (pattr->type == CStunClient::SOURCE_ADDRESS) {
        CStunClient::stun_addr_info one_address;
        CStunClient::cpAddress(pattr->value, &one_address);
        m_target_address.source_address = one_address.Address;
        m_target_address.source_port = one_address.port;
        U::SHOW_ADDR("TARGET SOURCE", one_address.Address, one_address.port);
      }
    }
    DPRINT(COMM, DEBUG_INFO, "GET [TARGET_RESPONSE]++\n\n");
    __OSAL_Event_Send(&m_sigTarget.hEvent);
  } else if (Type == CStunClient::SELECTION_UPDATE_RESPONSE) {
    DPRINT(COMM, DEBUG_INFO, "GET [SELECTION_UPDATE_RESPONSE]--\n");
    // TODO(Hyunduk): Do what you need for this response
    DPRINT(COMM, DEBUG_INFO, "GET [SELECTION_UPDATE_RESPONSE]++\n\n");
    __OSAL_Event_Send(&m_sigTarget.hEvent);
  }

  return TRUE;
}

BOOL CNetTunProc::ProcessTunPacket(int tun_msg_type,
                                   int tun_pkt_len,
                                   unsigned char* frame) {
  unsigned long source_address =
      frame[12] << 24 | frame[13] << 16 | frame[14] << 8 | frame[15];
  unsigned long destination_address =
      frame[16] << 24 | frame[17] << 16 | frame[18] << 8 | frame[19];
  unsigned short ihl = frame[0] & 0xF;

  unsigned short source_port =
      (frame[ihl * 4] << 8) | (frame[ihl * 4 + 1] << 8);
  unsigned short destination_port =
      (frame[ihl * 4 + 2] << 8) | (frame[ihl * 4 + 3] << 8);

  char request_buf[MAX_STUN_MSG_BUFF];
  int msg_len = 0;
  int retry = 0;

  DPRINT(COMM, DEBUG_INFO, "TUN PACKET PROCESS (%d Byte)--\n", tun_pkt_len);
  U::SHOW_ADDR("FROM", ntohl(source_address), ntohs(source_port));
  U::SHOW_ADDR("TO", ntohl(destination_address), ntohs(destination_port));

  U::SHOW_PACKET("Process Tun Packet", frame, tun_pkt_len);
  DPRINT(COMM, DEBUG_INFO, "Check Local Routing Map ==> (%s:%d)\n",
         U::CONV(ntohl(destination_address)), m_args.tun_port);

  CRouteTable::mapTable* pTableUnit =
      m_pTableHandler->QueryTable(ntohl(destination_address), m_args.tun_port,
                                  CRouteTable::QUERY_BY_SOURCEADDR);
  if (pTableUnit == NULL) {
    DPRINT(COMM, DEBUG_INFO, "No Local Routing Table Exist\n");
    DPRINT(COMM, DEBUG_INFO, "Send [MAPQUERY_REQUEST]\n");
    memset(request_buf, 0, MAX_STUN_MSG_BUFF);
    msg_len =
        CStunClient::bpRequest(request_buf, CStunClient::MAPQUERY_REQUEST,
                               ntohl(destination_address), m_args.tun_port);
    retry = m_args.retry_count;
    while (retry--) {
      m_pRemoteServer->DataSend(m_args.server_ip, (char*)request_buf, msg_len,
                                m_args.tun_port);
      if (__OSAL_Event_Wait(&m_sigQuery.hMutex, &m_sigQuery.hEvent,
                            m_args.time_unit) == OSAL_EVENT_WAIT_GETSIG) {
        DPRINT(COMM, DEBUG_INFO, "Get Mapped Address\n");
        break;
      }
    }
    if (retry <= 0) {
      DPRINT(COMM, DEBUG_INFO, "Cannot Receive [MAPQUERY_RESPONSE]\n");
      DPRINT(COMM, DEBUG_INFO, "TUN PACKET  : %d Byte\n", tun_pkt_len);
      return FALSE;
    }

    pTableUnit =
        m_pTableHandler->QueryTable(ntohl(destination_address), m_args.tun_port,
                                    CRouteTable::QUERY_BY_SOURCEADDR);
    if (pTableUnit == NULL) {
      DPRINT(COMM, DEBUG_INFO, "unknown error!!!!!!!!\n");
      DPRINT(COMM, DEBUG_INFO, "TUN PACKET PROCESS (FAIL)++\n\n");
      return FALSE;
    }
  } else {
    DPRINT(COMM, DEBUG_INFO, "Find Routing Table\n");
  }

  if (pTableUnit->type == CRouteTable::CONN_NOT_ESTABLISHED) {
    DPRINT(COMM, DEBUG_INFO, "Send [TRIAL_REQUEST]\n");
    memset(request_buf, 0, MAX_STUN_MSG_BUFF);
    msg_len = CStunClient::bpRequest(
        request_buf, CStunClient::TRIAL_REQUEST,
        m_device_address.source_address, m_device_address.source_port,
        m_device_address.mapped_address, m_device_address.mapped_port);
    retry = m_args.retry_count;
    while (retry--) {
      char* pszAddr = U::CONV(pTableUnit->mapped_address);
      m_pRemoteServer->DataSend(pszAddr, (char*)request_buf, msg_len,
                                pTableUnit->mapped_port);
      SAFE_DELETE(pszAddr);
      if (__OSAL_Event_Wait(&m_sigTrial.hMutex, &m_sigTrial.hEvent,
                            m_args.time_unit) == OSAL_EVENT_WAIT_GETSIG) {
        DPRINT(COMM, DEBUG_INFO, "Make STUN Connection\n");
        break;
      }
    }

    if (retry <= 0) {
      // allocated turn check
      DPRINT(COMM, DEBUG_INFO, "DIRECT CONNECTION IS NOT AVAILABLE\n");
      DPRINT(COMM, DEBUG_INFO, "ALLOCATE TURN CHANNEL\n");
      DPRINT(COMM, DEBUG_INFO, "SEND [TURNALLOC_REQUEST]\n");
      memset(request_buf, 0, MAX_STUN_MSG_BUFF);
      msg_len = CStunClient::bpRequest(
          request_buf, CStunClient::TURNALLOC_REQUEST,
          m_device_address.source_address, m_device_address.source_port,
          ntohl(destination_address), m_args.tun_port);
      retry = m_args.retry_count;
      while (retry--) {
        m_pRemoteServer->DataSend(m_args.server_ip, (char*)request_buf,
                                  msg_len);
        if (__OSAL_Event_Wait(&m_sigTurnAlloc.hMutex, &m_sigTurnAlloc.hEvent,
                              m_args.time_unit) == OSAL_EVENT_WAIT_GETSIG) {
          DPRINT(COMM, DEBUG_INFO, "ALLOCATE TURN CHANNEL SUCCESS\n");
          break;
        }
      }

      if (retry <= 0) {
        DPRINT(COMM, DEBUG_INFO, "ALLOCATE TURN CHANNEL FAIL\n");
        SAFE_DELETE(pTableUnit);
        DPRINT(COMM, DEBUG_INFO, "TUN PACKET PROCESS (FAIL)++\n\n");
        return FALSE;
      }
      // server�� data send
      CRouteTable::mapTable* pTurnTable = m_pTableHandler->QueryTable(
          ntohl(destination_address), m_args.tun_port,
          CRouteTable::QUERY_BY_SOURCEADDR);
      if (pTurnTable == NULL) {
        DPRINT(COMM, DEBUG_INFO, "FATAL ERROR --> TURN CHANNEL IS REMOVED\n");
        DPRINT(COMM, DEBUG_INFO, "TUN PACKET PROCESS (FAIL)++\n\n");
        return FALSE;
      } else {
        U::SHOW_TABLE(pTurnTable);
        char* pszAddr = U::CONV(pTurnTable->relay_address);
        U::SHOW_PACKET("turn data packet", frame, tun_pkt_len);
        DPRINT(COMM, DEBUG_INFO, "SEND IP FRAME TO RELAY SERVER(%s:%d)--\n",
               pszAddr, pTurnTable->relay_port);
        m_pRemoteServer->DataSend(pszAddr, (char*)frame, tun_pkt_len,
                                  pTableUnit->relay_port);
        SAFE_DELETE(pszAddr);

        m_pTableHandler->Access(pTurnTable->source_address,
                                pTableUnit->source_port);
        SAFE_DELETE(pTurnTable);
      }

    } else {
      char* pszAddr = U::CONV(pTableUnit->mapped_address);
      DPRINT(COMM, DEBUG_INFO, "SEND IP FRAME TO PEER DIRECTLY (%s:%d)\n",
             pszAddr, pTableUnit->mapped_port);
      m_pRemoteServer->DataSend(pszAddr, (char*)frame, tun_pkt_len,
                                pTableUnit->mapped_port);
      SAFE_DELETE(pszAddr);
      m_pTableHandler->Access(pTableUnit->source_address,
                              pTableUnit->source_port);
    }
  } else if (pTableUnit->type == CRouteTable::DIRECT_STUN_CONN) {
    char* pszAddr = U::CONV(pTableUnit->mapped_address);
    DPRINT(COMM, DEBUG_INFO, "SEND IP FRAME TO PEER DIRECTLY (%s:%d)\n",
           pszAddr, pTableUnit->mapped_port);
    m_pRemoteServer->DataSend(pszAddr, (char*)frame, tun_pkt_len,
                              pTableUnit->mapped_port);
    SAFE_DELETE(pszAddr);
    m_pTableHandler->Access(pTableUnit->source_address,
                            pTableUnit->source_port);
  } else if (pTableUnit->type == CRouteTable::RELAYED_TURN_CONN) {
    char* pszAddr = U::CONV(pTableUnit->relay_address);
    DPRINT(COMM, DEBUG_INFO, "SEND IP FRAME TO RELAY SERVER(%s:%d)--\n",
           pszAddr, pTableUnit->relay_port);
    m_pRemoteServer->DataSend(pszAddr, (char*)frame, tun_pkt_len,
                              pTableUnit->relay_port);
    SAFE_DELETE(pszAddr);
    m_pTableHandler->Access(pTableUnit->source_address,
                            pTableUnit->source_port);
  }
  SAFE_DELETE(pTableUnit);
  DPRINT(COMM, DEBUG_INFO, "TUN PACKET PROCESS ++\n\n");
  return TRUE;
}

/* Get From Physical NIC Message => Write to TUN Device*/

void CNetTunProc::OnRemoteMessage(int rmt_msg_type,
                                  char* addr,
                                  int port,
                                  int rmt_pkt_len,
                                  void* pData /*, void* pParents*/) {
  DPRINT(COMM, DEBUG_INFO, "REMOTE MESSAGE IN :[%s(%d)] - %d Byte\n", addr,
         port, rmt_pkt_len);
  U::SHOW_PACKET("remote message dump packet", (unsigned char*)pData,
                 rmt_pkt_len);
  CStunClient::STUN_MSG_TYPE Type;
  CbList<CStunClient::stun_msg_attr> attrList;
  char* __in_stream = (char*)pData;

  if (CStunClient::cpResponse(__in_stream, &Type, &attrList, rmt_pkt_len) < 0) {
    if (CStunClient::cpRequest(__in_stream, &Type, &attrList, rmt_pkt_len) <
        0) {
      int count = 3;
      while (count--) {
        INT32 wLen = m_hNetTunProc->m_pTunServer->DataSend((char*)__in_stream,
                                                           rmt_pkt_len);
        if (wLen > 0) {
          if (wLen == rmt_pkt_len) {
            // DPRINT(COMM, DEBUG_INFO,"IP PACKET => WRITE TUN %d
            // Byte\n\n",wLen);
            DPRINT(COMM, DEBUG_INFO,
                   "IP PACKET => [IP Seq - %2x, %2x] WRITE TUN [toWrite:%d "
                   "Byte] [Written:%d Byte]\n\n",
                   __in_stream[4], __in_stream[5], rmt_pkt_len, wLen);
            // DPRINT(COMM, DEBUG_INFO,"IP PACKET => [IP Seq - %2x, %2x] WRITE
            // TUN %d Byte\n\n",__in_stream[4], __in_stream[5], wLen);
            break;
          } else {
            DPRINT(
                COMM, DEBUG_ERROR,
                "TUN Write Fail--Driver Busy  [toWrite:%d] [Written:%d]!!!\n\n",
                rmt_pkt_len, wLen);
            __ASSERT(0);
          }
        } else {
          DPRINT(COMM, DEBUG_INFO, "IP PACKET => WRITE TUN Error\n");
          __OSAL_Sleep(1000);
        }
      }
      if (count <= 0) {
        DPRINT(COMM, DEBUG_ERROR, "TUN Write Fail--Check Configuration!!!\n\n");
        __ASSERT(0);
      }
    } else {
      DPRINT(COMM, DEBUG_INFO, "STUN REQUEST PACKET ==> now processing\n");
      m_hNetTunProc->ProcessRemotePacket(addr, port, Type, &attrList);
    }
  } else {
    DPRINT(COMM, DEBUG_INFO, "STUN RESPONSE PACKET ==>> now processing\n");
    m_hNetTunProc->ProcessRemotePacket(addr, port, Type, &attrList);
  }
}

/* Get From Tun Device => Send to Physical NIC */
void CNetTunProc::OnLocalMessage(int tun_msg_type,
                                 int tun_pkt_len,
                                 void* pData) {
  m_hNetTunProc->ProcessTunPacket(tun_msg_type, tun_pkt_len,
                                  (unsigned char*)pData);
}

VOID CNetTunProc::t_CreateEvent() {
  m_sigInit.hMutex = __OSAL_Mutex_Create();
  m_sigAlive.hMutex = __OSAL_Mutex_Create();
  m_sigTrial.hMutex = __OSAL_Mutex_Create();
  m_sigQuery.hMutex = __OSAL_Mutex_Create();
  m_sigTurnAlloc.hMutex = __OSAL_Mutex_Create();
  m_sigTarget.hMutex = __OSAL_Mutex_Create();
  m_sigSelectionUpdate.hMutex = __OSAL_Mutex_Create();

  m_sigInit.hEvent = __OSAL_Event_Create();
  m_sigAlive.hEvent = __OSAL_Event_Create();
  m_sigTrial.hEvent = __OSAL_Event_Create();
  m_sigQuery.hEvent = __OSAL_Event_Create();
  m_sigTurnAlloc.hEvent = __OSAL_Event_Create();
  m_sigTarget.hEvent = __OSAL_Event_Create();
  m_sigSelectionUpdate.hEvent = __OSAL_Event_Create();
}

VOID CNetTunProc::t_DestroyEvent() {
  __OSAL_Mutex_Destroy(&m_sigInit.hMutex);
  __OSAL_Mutex_Destroy(&m_sigAlive.hMutex);
  __OSAL_Mutex_Destroy(&m_sigTrial.hMutex);
  __OSAL_Mutex_Destroy(&m_sigQuery.hMutex);
  __OSAL_Mutex_Destroy(&m_sigTurnAlloc.hMutex);
  __OSAL_Mutex_Destroy(&m_sigTarget.hMutex);
  __OSAL_Mutex_Destroy(&m_sigSelectionUpdate.hMutex);

  __OSAL_Event_Destroy(&m_sigInit.hEvent);
  __OSAL_Event_Destroy(&m_sigAlive.hEvent);
  __OSAL_Event_Destroy(&m_sigTrial.hEvent);
  __OSAL_Event_Destroy(&m_sigQuery.hEvent);
  __OSAL_Event_Destroy(&m_sigTurnAlloc.hEvent);
  __OSAL_Event_Destroy(&m_sigTarget.hEvent);
  __OSAL_Event_Destroy(&m_sigSelectionUpdate.hEvent);
}

BOOL CNetTunProc::t_ProcessDHCP() {
  int retryCount = m_args.retry_count;
  char buff[MAX_STUN_MSG_BUFF];
  memset(buff, 0, MAX_STUN_MSG_BUFF);
  int toSend = CStunClient::bpRequest(buff, CStunClient::DHCP_REQUEST,
                                      m_device_address.source_address,
                                      m_device_address.source_port);
  while (retryCount > 0) {
    U::SHOW_PACKET("dhcp request packet", (unsigned char*)buff, toSend);
    m_pRemoteServer->DataSend(m_args.server_ip, (char*)buff, toSend,
                              m_args.tun_port);
    OSAL_Event_Status ret = __OSAL_Event_Wait(
        &m_sigInit.hMutex, &m_sigInit.hEvent, m_args.time_unit);
    if (ret == OSAL_EVENT_WAIT_GETSIG) {
      DPRINT(COMM, DEBUG_INFO, "Address Allocation Success\n");
      return TRUE;
    } else if (ret == OSAL_EVENT_WAIT_TIMEOUT) {
      DPRINT(COMM, DEBUG_INFO, "DHCP Wait Timeout - retry (%d)th more--\n",
             retryCount);
    } else {
      DPRINT(COMM, DEBUG_INFO, "OSAL Wait Unknwon Error\n");
    }
    retryCount--;
  }
  return FALSE;
}

unsigned long CNetTunProc::GetTarget() {
  if (m_hasTarget) {
    return m_target_address.mapped_address;
  }
  return 0;
}

static int temp_count = 0;

void CNetTunProc::MainLoop(void* args) {
  UINT64 last_send = 0;
  UINT64 current_time = 0;
  while (CbThread::m_bRun) {
    CheckEvent();

    if (m_device_address.source_address == 0) {
      __OSAL_Sleep(1000);
      continue;
    }

    __OSAL_TIME_GetTimeMS(&current_time);

#ifdef LEESS
    if (temp_count < 10)
      m_args.bind_period = 1000 * 5;
    else
      m_args.bind_period = 1000 * 60 * 5;

    if ((current_time - last_send) > (UINT32)m_args.bind_period) {
      printf("LEESS: ");
      printf("%d\n", m_args.bind_period);

#else
    if ((current_time - last_send) > (UINT32)m_args.bind_period) {
#endif

      char buff[MAX_STUN_MSG_BUFF];
      memset(buff, 0, MAX_STUN_MSG_BUFF);
      int toSend = CStunClient::bpRequest(buff, CStunClient::BINDING_REQUEST,
                                          m_device_address.source_address,
                                          m_device_address.source_port);

      int retryCount = m_args.retry_count;
      while (retryCount--) {
        m_pRemoteServer->DataSend(m_args.server_ip, (char*)buff, toSend);
        OSAL_Event_Status ret = __OSAL_Event_Wait(
            &m_sigAlive.hMutex, &m_sigAlive.hEvent, m_args.time_unit);
        if (ret == OSAL_EVENT_WAIT_GETSIG) {
          break;
        } else if (ret == OSAL_EVENT_WAIT_TIMEOUT) {
          DPRINT(COMM, DEBUG_INFO,
                 "Binding Response Timeout - retry (%d)th more\n", retryCount);
        }
      }

      CStunClient::STUN_MSG_TYPE req_msg =
          (m_args.role == CRouteTable::BROWSER) ?
          CStunClient::TARGETR_REQUEST : CStunClient::TARGETB_REQUEST;
      memset(buff, 0, MAX_STUN_MSG_BUFF);
      toSend = CStunClient::bpRequest(buff, req_msg,
                                          m_device_address.source_address,
                                          m_device_address.source_port);

      retryCount = m_args.retry_count;
      while (retryCount--) {
        m_pRemoteServer->DataSend(m_args.server_ip, (char*)buff, toSend);
        OSAL_Event_Status ret = __OSAL_Event_Wait(
            &m_sigTarget.hMutex, &m_sigTarget.hEvent, m_args.time_unit);
        if (ret == OSAL_EVENT_WAIT_GETSIG) {
          break;
        } else if (ret == OSAL_EVENT_WAIT_TIMEOUT) {
          DPRINT(COMM, DEBUG_INFO,
                 "Target Response Timeout - retry (%d)th more\n", retryCount);
        }
      }

      last_send = current_time;
#ifndef LEESS
      temp_count++;
#endif
    }
  }
}
