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

#include "NetworkService.h"

#include "netUtil.h"
#include "string_util.h"

using namespace mmBase;
using namespace mmProto;

static int alloc_table[100] = {
    0,
};

CNetworkService::CNetworkService(const CHAR* msgqname,
                                 char* pszBindAddress,
                                 unsigned short stun_port)
    : CpUdpServer(msgqname) {
  m_pszBindServerAddress = new char[strlen(pszBindAddress) + 1];
  strlcpy(m_pszBindServerAddress, pszBindAddress,
                  sizeof(m_pszBindServerAddress));
  m_stun_port = stun_port;

  CpUdpServer::Create();
  m_pRoutingTable = new CRouteTable("RoutingTableHandler");
}

CNetworkService::~CNetworkService() {
  CpUdpServer::Destroy();
  SAFE_DELETE(m_pRoutingTable);
  SAFE_DELETE(m_pszBindServerAddress);
}

BOOL CNetworkService::StartServer(int port, int readperonce) {
  CpUdpServer::Open(port);
  CpUdpServer::Start(readperonce);
  m_pRoutingTable->RouteTableCheckerStart();
  DPRINT(COMM, DEBUG_INFO, "start remote server with [%d] port\n", port);
  return TRUE;
}

BOOL CNetworkService::StopServer() {
  CpUdpServer::Stop();
  return TRUE;
}

unsigned long CNetworkService::GetFreeAddress() {
  int i = 0;
  for (i = 0; i < 100; i++) {
    if (!alloc_table[i]) {
      alloc_table[i] = 1;
      break;
    }
  }

  char l_addr[16];
  memset(l_addr, 0, 16);
  sprintf(l_addr, "10.10.10.%d", i + 2);

  struct in_addr laddr;

#ifdef WIN32
  inet_pton(AF_INET, l_addr, &laddr);
#else
  inet_aton(l_addr, &laddr);
#endif

  unsigned long alloc_addr = laddr.s_addr;

  return alloc_addr;
}

VOID CNetworkService::DataRecv(OSAL_Socket_Handle iEventSock,
                               const CHAR* pszsource_addr,
                               long source_port,
                               CHAR* pData,
                               INT32 iLen) {
  DPRINT(COMM, DEBUG_INFO, "DATA IN - from:[%s(%d)] [%d] Byte\n",
         pszsource_addr, source_port, iLen);

  U::SHOW_PACKET((char*)"receive data", (unsigned char*)pData, iLen);

  CStunClient::STUN_MSG_TYPE Type;
  CbList<CStunClient::stun_msg_attr> attrList;
  char response_buf[1024];
  int response_msglen = 0;
  if (CStunClient::cpRequest((char*)pData, &Type, &attrList, iLen) < 0) {
    // It is not stun/dhcp req packet
    unsigned char* frame = (unsigned char*)pData;
    //		unsigned short ihl=frame[0]&0xF;
    unsigned long i_src_addr =
        ntohl(frame[12] << 24 | frame[13] << 16 | frame[14] << 8 | frame[15]);
    unsigned long i_dest_addr =
        ntohl(frame[16] << 24 | frame[17] << 16 | frame[18] << 8 | frame[19]);
    /*
                    unsigned short
       source_port=(frame[ihl*4]<<8)|(frame[ihl*4+1]<<8);
                    unsigned short
       destination_port=(frame[ihl*4+2]<<8)|(frame[ihl*4+3]<<8);
    */

    char* pszSrcAddr = U::CONV(i_src_addr);
    char* pszDestAddr = U::CONV(i_dest_addr);
    DPRINT(COMM, DEBUG_INFO, "RELAY PACKET IN FROM %s TO %s %d byte\n",
           pszSrcAddr, pszDestAddr, iLen);
    CRouteTable::turnTable* pc =
        m_pRoutingTable->QueryChannel(i_src_addr, i_dest_addr);
    if (pc == NULL) {
      DPRINT(COMM, DEBUG_INFO, "Turn Channel(%s<->%s) is not Allocated\n",
             pszSrcAddr, pszDestAddr);

      SAFE_DELETE(pszSrcAddr);
      SAFE_DELETE(pszDestAddr);
    } else {
      DPRINT(COMM, DEBUG_INFO, "Turn Channel(%s<->%s) is Allocated\n",
             pszSrcAddr, pszDestAddr);

      CRouteTable::mapTable* pr = m_pRoutingTable->QueryTable(
          i_dest_addr, m_stun_port, CRouteTable::QUERY_BY_SOURCEADDR);
      if (pr == NULL) {
        DPRINT(COMM, DEBUG_INFO, "Destination(%s) is Not STUN registered\n",
               pszDestAddr);

      } else {
        char* pszMappedDest = U::CONV(pr->mapped_address);
        char* pszSourceDest = U::CONV(pr->source_address);
        DataSend(pszMappedDest, pData, iLen, pr->mapped_port);
        m_pRoutingTable->Access(i_src_addr, i_dest_addr);
        DPRINT(COMM, DEBUG_INFO,
               "<== COMPLETE DATA RELAY TO "
               "[SOURCE-(%s):(%d)]-[MAPPED-(%s):(%d)] \n",
               pszSourceDest, pr->source_port, pszMappedDest, pr->mapped_port);
        SAFE_DELETE(pszMappedDest);
        SAFE_DELETE(pszSourceDest);
        SAFE_DELETE(pr);
      }
      SAFE_DELETE(pc);
    }
    SAFE_DELETE(pszSrcAddr);
    SAFE_DELETE(pszDestAddr);

  } else {
    // It is stun or dhcp req packet
    if (Type == CStunClient::MAPQUERY_REQUEST) {
      DPRINT(COMM, DEBUG_INFO, "GET [MAPQUERY_REQUEST]--\n");
      CRouteTable::mapTable map;
      int len = attrList.GetCount();
      for (int i = 0; i < len; i++) {
        CStunClient::stun_msg_attr* pattr = attrList.GetAt(i);
        if (pattr->type == CStunClient::SOURCE_ADDRESS) {
          CStunClient::stun_addr_info one_address;
          CStunClient::cpAddress(pattr->value, &one_address);
          map.source_address = one_address.Address;
          map.source_port = one_address.port;
        } else if (pattr->type == CStunClient::MAPPED_ADDRESS) {
          CStunClient::stun_addr_info one_address;
          CStunClient::cpAddress(pattr->value, &one_address);
          map.mapped_address = one_address.Address;
          map.mapped_port = one_address.port;
        }
      }

      CRouteTable::mapTable* p =
          m_pRoutingTable->QueryTable(map.source_address, map.source_port,
                                      CRouteTable::QUERY_BY_SOURCEADDR);
      if (p == NULL) {
        // MAPQUERY_ERROR_RESPOND
        DPRINT(COMM, DEBUG_INFO, "response Error=>\n");
        U::SHOW_ADDR("MAPPED", map.source_address, map.source_port);
        DPRINT(COMM, DEBUG_INFO, "<= response Error!!\n");
        memset(response_buf, 0, 1024);
        response_msglen = CStunClient::bpRequest(
            response_buf, CStunClient::MAPQUERY_ERROR_RESPONSE, 0, 0, 0, 0);

      } else {
        DPRINT(COMM, DEBUG_INFO, "response success=>\n");
        U::SHOW_TABLE(p);
        DPRINT(COMM, DEBUG_INFO, "<= response success!!\n");
        memset(response_buf, 0, 1024);
        response_msglen = CStunClient::bpRequest(
            response_buf, CStunClient::MAPQUERY_RESPONSE, p->source_address,
            p->source_port, p->mapped_address, p->mapped_port);
        SAFE_DELETE(p);
      }

      DataSend(pszsource_addr, response_buf, response_msglen, source_port);
      DPRINT(COMM, DEBUG_INFO, "SEND RESPONSE %s(%d)\n", pszsource_addr,
             source_port);
      DPRINT(COMM, DEBUG_INFO, "GET [MAPQUERY_REQUEST]++\n");
    } else if (Type == CStunClient::DHCP_REQUEST) {
      DPRINT(COMM, DEBUG_INFO, "GET [DHCP_REQUEST]--\n");
      unsigned long alloc_addr = GetFreeAddress();
      memset(response_buf, 0, 1024);
      response_msglen =
          CStunClient::bpRequest(response_buf, CStunClient::DHCP_RESPONSE,
                                 alloc_addr, m_stun_port, 0, 0);

      char* psz = U::CONV(alloc_addr);
      DPRINT(COMM, DEBUG_INFO, "Alloc[%s:5000]\n", psz);
      DPRINT(COMM, DEBUG_INFO, "Data Send to %s %d\n", pszsource_addr,
             source_port);
      DataSend(pszsource_addr, response_buf, response_msglen, source_port);
      DPRINT(COMM, DEBUG_INFO, "SEND RESPONSE : %s(%d)\n", pszsource_addr,
             source_port);
      DPRINT(COMM, DEBUG_INFO, "GET [DHCP_REQUEST]++\n");

    } else if (Type == CStunClient::BINDING_REQUEST) {
      // CStunClient::stun_addr_info one_address;
      DPRINT(COMM, DEBUG_INFO, "GET [BINDING_REQUEST]--\n");
      int len = attrList.GetCount();
      for (int i = 0; i < len; i++) {
        CStunClient::stun_msg_attr* pattr = attrList.GetAt(i);
        if (pattr->type == CStunClient::SOURCE_ADDRESS) {
          unsigned long ui_addr = 0;
          inet_pton(AF_INET, pszsource_addr, (void*)&ui_addr);
          CStunClient::stun_addr_info one_address;
          CStunClient::cpAddress(pattr->value, &one_address);
          CRouteTable::mapTable* p =
              m_pRoutingTable->QueryTable(one_address.Address, one_address.port,
                                          CRouteTable::QUERY_BY_SOURCEADDR);
          if (p == NULL) {
            p = new CRouteTable::mapTable;
            p->mapped_address = ui_addr;
            p->mapped_port = (unsigned short)source_port;
            p->source_address = one_address.Address;
            p->source_port = one_address.port;
            p->matched_address = 0;
            p->matched_port = 0;
            p->matched_role = CRouteTable::NONE;
            p->capable_role = CRouteTable::NONE;
            m_pRoutingTable->AddPath(p);
            DPRINT(COMM, DEBUG_INFO, "Add Table\n");
            U::SHOW_TABLE(p);
          } else {
            if ((p->mapped_address == ui_addr) &&
                (p->mapped_port == source_port)) {
              // DPRINT(COMM,DEBUG_INFO,"Matched Table Already Exist!!!\n");
            } else {
              p->mapped_address = ui_addr;
              p->mapped_port = (unsigned short)source_port;
              m_pRoutingTable->UpdateTable(one_address.Address,
                                           one_address.port, p);
              DPRINT(COMM, DEBUG_INFO, "Update Table\n");
              U::SHOW_TABLE(p);
            }
            SAFE_DELETE(p);
          }
          memset(response_buf, 0, 1024);
          response_msglen = CStunClient::bpRequest(
              response_buf, CStunClient::BINDING_RESPONSE, one_address.Address,
              one_address.port, ui_addr, (unsigned short)source_port);
          DataSend(pszsource_addr, response_buf, response_msglen, source_port);
          DPRINT(COMM, DEBUG_INFO, "SEND RESPONSE %s(%d)\n", pszsource_addr,
                 source_port);
          m_pRoutingTable->Access(one_address.Address, one_address.port);
          break;
        }
      }
      DPRINT(COMM, DEBUG_INFO, "GET [BINDING_REQUEST]++\n");
    } else if (Type == CStunClient::TURNALLOC_REQUEST) {
      DPRINT(COMM, DEBUG_INFO, "GET [TURNALLOC_REQUEST]--\n");
      CRouteTable::mapTable map;
      int len = attrList.GetCount();
      for (int i = 0; i < len; i++) {
        CStunClient::stun_msg_attr* pattr = attrList.GetAt(i);
        if (pattr->type == CStunClient::SOURCE_ADDRESS) {
          CStunClient::stun_addr_info one_address;
          CStunClient::cpAddress(pattr->value, &one_address);
          map.source_address = one_address.Address;
          map.source_port = one_address.port;
        } else if (pattr->type == CStunClient::MAPPED_ADDRESS) {
          CStunClient::stun_addr_info one_address;
          CStunClient::cpAddress(pattr->value, &one_address);
          map.mapped_address = one_address.Address;
          map.mapped_port = one_address.port;
        }
      }
      CRouteTable::mapTable* p =
          m_pRoutingTable->QueryTable(map.mapped_address, map.mapped_port,
                                      CRouteTable::QUERY_BY_SOURCEADDR);
      if (p == NULL) {
        // TURNALLOC_ERROR_RESPOND
        DPRINT(COMM, DEBUG_INFO, "response Error=>\n");
        U::SHOW_ADDR("FROM", map.source_address, map.source_port);
        U::SHOW_ADDR("TO", map.mapped_address, map.mapped_port);
        DPRINT(COMM, DEBUG_INFO, "<= response Error!!\n");
        memset(response_buf, 0, 1024);
        response_msglen = CStunClient::bpRequest(
            response_buf, CStunClient::TURNALLOC_ERROR_RESPONSE, 0, 0, 0, 0);

      } else {
        // TURNALLOC_RESPOND
        DPRINT(COMM, DEBUG_INFO, "response success=>\n");
        U::SHOW_ADDR("FROM", map.source_address, map.source_port);
        U::SHOW_ADDR("TO", map.mapped_address, map.mapped_port);
        DPRINT(COMM, DEBUG_INFO, "<= response success!!\n");
        memset(response_buf, 0, 1024);

        unsigned long ui_addr = 0;
        inet_pton(AF_INET,
                  m_pszBindServerAddress, (void*)&ui_addr); /* Grid Server
                                                               연결시 여기에
                                                               Load Balancing
                                                               routine 추가 */
        response_msglen = CStunClient::bpRequest(
            response_buf, CStunClient::TURNALLOC_RESPONSE, p->source_address,
            p->source_port, ui_addr, m_stun_port);

        CRouteTable::turnTable* pcc = new CRouteTable::turnTable;
        pcc->endpoint[0] = map.source_address;
        pcc->endpoint[1] = map.mapped_address;
        pcc->relaypoint = ui_addr;

        if (!m_pRoutingTable->AddChannel(pcc))
          SAFE_DELETE(pcc);
        SAFE_DELETE(p);
      }

      DataSend(pszsource_addr, response_buf, response_msglen, source_port);
      DPRINT(COMM, DEBUG_INFO, "SEND RESPONSE %s(%d)\n", pszsource_addr,
             source_port);
      DPRINT(COMM, DEBUG_INFO, "GET [TURNALLOC_REQUEST]++\n");
    } else if (Type == CStunClient::TARGETB_REQUEST ||
               Type == CStunClient::TARGETR_REQUEST) {
      DPRINT(COMM, DEBUG_INFO, "GET [TARGET_REQUEST]--\n");
      int len = attrList.GetCount();
      bool found = false;
      CRouteTable::role_type role = (Type == CStunClient::TARGETR_REQUEST) ?
          CRouteTable::BROWSER : CRouteTable::RENDERER;
      for (int i = 0; i < len; i++) {
        CStunClient::stun_msg_attr* pattr = attrList.GetAt(i);
        if (pattr->type == CStunClient::SOURCE_ADDRESS) {
          unsigned long ui_addr = 0;
          unsigned short ui_port = (unsigned short)source_port;
          inet_pton(AF_INET, pszsource_addr, (void*)&ui_addr);
          CStunClient::stun_addr_info one_address;
          CStunClient::cpAddress(pattr->value, &one_address);
          CRouteTable::mapTable* p =
              m_pRoutingTable->QueryTable(one_address.Address, one_address.port,
                                          CRouteTable::QUERY_BY_SOURCEADDR);
          CRouteTable::mapTable* pt =
              m_pRoutingTable->QueryTarget(one_address.Address, role);
          if (p == NULL) {
            DPRINT(COMM, DEBUG_INFO,
                   "Client is not registered in table\n");
          } else {
            if ((p->mapped_address == ui_addr) &&
                (p->mapped_port == source_port)) {
              if (pt != NULL) found = true;
            } else {
              DPRINT(COMM, DEBUG_INFO,
                     "Mapped addr or port of client has been changed\n");
            }
            if (found) {
              DPRINT(COMM,DEBUG_INFO,"Matched Table Found!!!\n");
              one_address.Address = pt->source_address;
              one_address.port = pt->source_port;
              ui_addr = pt->mapped_address;
              ui_port = pt->mapped_port;
            }
            SAFE_DELETE(p);
            SAFE_DELETE(pt);
          }
          CStunClient::STUN_MSG_TYPE res_msg =
              (Type == CStunClient::TARGETR_REQUEST) ?
              CStunClient::TARGETR_RESPONSE : CStunClient::TARGETB_RESPONSE;
          memset(response_buf, 0, 1024);
          response_msglen = CStunClient::bpRequest(
              response_buf, res_msg, one_address.Address,
              one_address.port, ui_addr, ui_port);
          DataSend(pszsource_addr, response_buf, response_msglen, source_port);
          DPRINT(COMM, DEBUG_INFO, "SEND RESPONSE %s(%d)\n", pszsource_addr,
                 source_port);
          m_pRoutingTable->Access(one_address.Address, one_address.port);
          break;
        }
      }
      DPRINT(COMM, DEBUG_INFO, "GET [TARGET_REQUEST]++\n");
    } else if (Type == CStunClient::SELECTION_UPDATE_REQUEST) {
      DPRINT(COMM, DEBUG_INFO, "GET [SELECTION_UPDATE_REQUEST]--\n");
      // TODO(Hyunduk) : Do what you need to do and respond to this request
      DPRINT(COMM, DEBUG_INFO, "GET [SELECTION_UPDATE_REQUEST]++\n");
    }
  }
}

VOID CNetworkService::EventNotify(OSAL_Socket_Handle eventSock,
                                  CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify- form:sock[%d] event[%d]\n", eventSock,
         type);
}

INT32 CNetworkService::MEMDUMP_TABLE(PCHAR bucket[]) {
  int cnt = 0;
  cnt = m_pRoutingTable->MEMDUMP_T(bucket);
  for (int i = 0; i < cnt; i++) {
    DPRINT(COMM, DEBUG_INFO, "MEMDUMP_TABLE-no[%d] source addr[%s]\n", i,
           bucket[i * 4]);
    DPRINT(COMM, DEBUG_INFO, "MEMDUMP_TABLE-no[%d] mapped addr[%s]\n", i,
           bucket[i * 4 + 1]);
    DPRINT(COMM, DEBUG_INFO, "MEMDUMP_TABLE-no[%d] matched addr[%s]\n", i,
           bucket[i * 4 + 2]);
    DPRINT(COMM, DEBUG_INFO, "MEMDUMP_TABLE-no[%d] matched role[%s]\n", i,
           bucket[i * 4 + 3]);
  }
  return cnt;
}
