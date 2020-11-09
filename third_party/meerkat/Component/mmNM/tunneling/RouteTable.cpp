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

#include "netUtil.h"
//#include "timeAPI.h"
//#include "osal.h"

using namespace mmBase;

CRouteTable::CRouteTable(const char* pszName) : CbThread(pszName) {
  m_accessKey = __OSAL_Mutex_Create();
  m_channelKey = __OSAL_Mutex_Create();
}

CRouteTable::~CRouteTable() {
  __OSAL_Mutex_Destroy(&m_accessKey);
  __OSAL_Mutex_Destroy(&m_channelKey);
}

BOOL CRouteTable::RouteTableCheckerStart() {
  CbThread::StartMainLoop(NULL);
  return TRUE;
}

BOOL CRouteTable::RouteTableCheckerStop() {
  CbThread::StopMainLoop();
  return TRUE;
}

BOOL CRouteTable::AddChannel(turnTable* pTable) {
  __OSAL_Mutex_Lock(&m_channelKey);

  int count = m_LocalTurnTable.GetCount();
  for (int i = 0; i < count; i++) {
    turnTable* ptt = m_LocalTurnTable.GetAt(i);
    if (((ptt->endpoint[0] == pTable->endpoint[0]) &&
         (ptt->endpoint[1] == pTable->endpoint[1])) ||
        ((ptt->endpoint[0] == pTable->endpoint[1]) &&
         (ptt->endpoint[1] == pTable->endpoint[0]))) {
      __OSAL_Mutex_UnLock(&m_channelKey);
      return FALSE;
    }
  }
  m_LocalTurnTable.AddTail(pTable);
  __OSAL_Mutex_UnLock(&m_channelKey);
  return TRUE;
}

BOOL CRouteTable::DelChannel(turnTable* pTable) {
  __OSAL_Mutex_Lock(&m_channelKey);

  int count = m_LocalTurnTable.GetCount();
  for (int i = 0; i < count; i++) {
    turnTable* ptt = m_LocalTurnTable.GetAt(i);
    if (((ptt->endpoint[0] == pTable->endpoint[0]) &&
         (ptt->endpoint[1] == pTable->endpoint[1])) ||
        ((ptt->endpoint[0] == pTable->endpoint[1]) &&
         (ptt->endpoint[1] == pTable->endpoint[0]))) {
      m_LocalTurnTable.DelAt(i);
      __OSAL_Mutex_UnLock(&m_channelKey);
      return TRUE;
    }
  }

  __OSAL_Mutex_UnLock(&m_channelKey);
  return FALSE;
}

CRouteTable::turnTable* CRouteTable::QueryChannel(unsigned long ep0,
                                                  unsigned long ep1) {
  __OSAL_Mutex_Lock(&m_channelKey);
  int count = m_LocalTurnTable.GetCount();
  for (int i = 0; i < count; i++) {
    turnTable* ptt = m_LocalTurnTable.GetAt(i);
    if (((ptt->endpoint[0] == ep0) && (ptt->endpoint[1] == ep1)) ||
        ((ptt->endpoint[0] == ep1) && (ptt->endpoint[1] == ep0))) {
      turnTable* p = new turnTable;
      p->endpoint[0] = ptt->endpoint[0];
      p->endpoint[1] = ptt->endpoint[1];
      p->relaypoint = ptt->relaypoint;
      p->last_connect_time = ptt->last_connect_time;
      __OSAL_Mutex_UnLock(&m_channelKey);
      return p;
    }
  }
  __OSAL_Mutex_UnLock(&m_channelKey);
  return NULL;
}

BOOL CRouteTable::Access(unsigned long ep0, unsigned long ep1) {
  __OSAL_Mutex_Lock(&m_channelKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    turnTable* ptt = m_LocalTurnTable.GetAt(i);
    if (((ptt->endpoint[0] == ep0) && (ptt->endpoint[1] == ep1)) ||
        ((ptt->endpoint[0] == ep1) && (ptt->endpoint[1] == ep0))) {
      __OSAL_TIME_GetTimeMS((UINT64*)&ptt->last_connect_time);
      __OSAL_Mutex_UnLock(&m_channelKey);
      return TRUE;
    }
  }
  __OSAL_Mutex_UnLock(&m_channelKey);
  return FALSE;
}

BOOL CRouteTable::AddPath(mapTable* pTable) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::AddPath--\n");
  __OSAL_Mutex_Lock(&m_accessKey);

  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if ((ptt->source_address == pTable->source_address) &&
        (ptt->source_port == pTable->source_port)) {
      m_LocalRoutingTable.DelAt(i);
      break;
    }
  }
  __OSAL_TIME_GetTimeMS((UINT64*)&pTable->last_connect_time);
  m_LocalRoutingTable.AddTail(pTable);
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::AddPath++\n");
  return TRUE;
}

BOOL CRouteTable::DelPath(unsigned long source_address,
                          unsigned short source_port) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::DelPath--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if ((ptt->source_address == source_address) &&
        (ptt->source_port == source_port)) {
      m_LocalRoutingTable.DelAt(i);
      __OSAL_Mutex_UnLock(&m_accessKey);
      DPRINT(COMM, DEBUG_INFO, "CRouteTable::DelPath++\n");
      return TRUE;
    }
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::DelPath++\n");
  return FALSE;
}

CRouteTable::mapTable* CRouteTable::QueryTable(unsigned long address,
                                               unsigned short port,
                                               querykey_type type) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTable--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if (type == QUERY_BY_SOURCEADDR) {
      if ((ptt->source_address == address) && (ptt->source_port == port)) {
        mapTable* p = new mapTable;
        CopyTable(ptt, p);
        __OSAL_Mutex_UnLock(&m_accessKey);
        DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTable++\n");
        return p;
      }
    } else {
      if ((ptt->mapped_address == address) && (ptt->mapped_port == port)) {
        mapTable* p = new mapTable;
        CopyTable(ptt, p);
        __OSAL_Mutex_UnLock(&m_accessKey);
        DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTable++\n");
        return p;
      }
    }
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTable++\n");
  return NULL;
}

CRouteTable::mapTable* CRouteTable::QueryTarget(unsigned long address,
                                                role_type role) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTarget--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  int my_pos = 0;

  // Get requester's index in table and set the matched role
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if (ptt->source_address == address) {
      my_pos = i;
      if (ptt->matched_role == NONE) {
        ptt->matched_role = role;
      }
      break;
    }
  }

  // Finding a renderer
  if (role == BROWSER) {
    for (int i = 0; i < count; i++) {
      mapTable* ptt = m_LocalRoutingTable.GetAt(i);
      if (ptt->source_address != address) {
        if ((ptt->matched_address == 0) && (ptt->matched_role == RENDERER)) {
          // Set matched renderer info in browser
          mapTable* pts = m_LocalRoutingTable.GetAt(my_pos);
          pts->matched_address = ptt->source_address;
          pts->matched_port = ptt->source_port;

          // Set matched browser info in renderer
          ptt->matched_address = pts->source_address;
          ptt->matched_port = pts->source_port;

          mapTable* p = new mapTable;
          CopyTable(ptt, p);
          __OSAL_Mutex_UnLock(&m_accessKey);
          DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTarget++1\n");
          return p;
        } else if (ptt->matched_address == address) {
          mapTable* p = new mapTable;
          CopyTable(ptt, p);
          __OSAL_Mutex_UnLock(&m_accessKey);
          DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTarget++2\n");
          return p;
        }
      }
    }
  } else if (role == RENDERER) {
    for (int i = 0; i < count; i++) {
      mapTable* ptt = m_LocalRoutingTable.GetAt(i);
      if (ptt->source_address != address) {
        if (ptt->matched_address == address) {
          mapTable* p = new mapTable;
          CopyTable(ptt, p);
          __OSAL_Mutex_UnLock(&m_accessKey);
          DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTarget++3\n");
          return p;
        }
      }
    }
  }

  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::QueryTarget++\n");
  return NULL;
}

BOOL CRouteTable::UpdateTable(unsigned long sourceaddr,
                              unsigned short port,
                              mapTable* p) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::UpdateTable--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);

    if ((ptt->source_address == sourceaddr) && (ptt->source_port == port)) {
      CopyTable(p, ptt);
      __OSAL_Mutex_UnLock(&m_accessKey);
      DPRINT(COMM, DEBUG_INFO, "CRouteTable::UpdateTable++\n");
      return TRUE;
    }
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::UpdateTable++\n");
  return FALSE;
}

BOOL CRouteTable::SetConnType(unsigned long address,
                              unsigned short port,
                              connection_type type) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::SetConnType--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if ((ptt->source_address == address) && (ptt->source_port == port)) {
      ptt->type = type;
      __OSAL_Mutex_UnLock(&m_accessKey);
      DPRINT(COMM, DEBUG_INFO, "CRouteTable::SetConnType++\n");
      return TRUE;
    }
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::SetConnType++\n");
  return FALSE;
}

BOOL CRouteTable::SetChannelState(unsigned long address,
                                  unsigned short port,
                                  channel_state state) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::SetChannelState--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if ((ptt->source_address == address) && (ptt->source_port == port)) {
      ptt->state = state;
      __OSAL_Mutex_UnLock(&m_accessKey);
      DPRINT(COMM, DEBUG_INFO, "CRouteTable::SetChannelState++\n");
      return TRUE;
    }
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::SetChannelState++\n");
  return FALSE;
}

BOOL CRouteTable::Access(unsigned long address, unsigned short port) {
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::Access--\n");
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    if ((ptt->source_address == address) && (ptt->source_port == port)) {
      __OSAL_TIME_GetTimeMS((UINT64*)&ptt->last_connect_time);
      __OSAL_Mutex_UnLock(&m_accessKey);
      DPRINT(COMM, DEBUG_INFO, "CRouteTable::Access++\n");
      return TRUE;
    }
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  DPRINT(COMM, DEBUG_INFO, "CRouteTable::Access++\n");
  return FALSE;
}

VOID CRouteTable::DUMP_T() {
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  DPRINT(COMM, DEBUG_INFO, "<%d MAP TABLE EXIST>\n", count);
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    U::SHOW_TABLE(ptt);
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
}

INT32 CRouteTable::MEMDUMP_T(PCHAR bucket[]) {
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalRoutingTable.GetCount();
  DPRINT(COMM, DEBUG_INFO, "<MEMDUMP : %d MAP TABLE EXIST>\n", count);
  for (int i = 0; i < count; i++) {
    mapTable* ptt = m_LocalRoutingTable.GetAt(i);
    bucket[i * 4] = U::GET_TABLE(ptt, 0);      // source addr
    bucket[i * 4 + 1] = U::GET_TABLE(ptt, 1);  // mapped addr
    bucket[i * 4 + 2] = U::GET_TABLE(ptt, 2);  // matched addr
    bucket[i * 4 + 3] = U::GET_TABLE(ptt, 3);  // matched role
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
  return count;
}

VOID CRouteTable::DUMP_C() {
  __OSAL_Mutex_Lock(&m_accessKey);
  int count = m_LocalTurnTable.GetCount();
  DPRINT(COMM, DEBUG_INFO, "<%d CHANNEL TABLE EXIST>\n", count);
  for (int i = 0; i < count; i++) {
    turnTable* ptt = m_LocalTurnTable.GetAt(i);
    U::SHOW_ADDR("ep1", ptt->endpoint[0], 5000);
    U::SHOW_ADDR("ep2", ptt->endpoint[1], 5000);
    U::SHOW_ADDR("rel", ptt->relaypoint, 5000);
  }
  __OSAL_Mutex_UnLock(&m_accessKey);
}

void CRouteTable::CopyTable(mapTable* src, mapTable* dst) {
  dst->source_address = src->source_address;
  dst->source_port = src->source_port;
  dst->mapped_address = src->mapped_address;
  dst->mapped_port = src->mapped_port;
  dst->relay_address = src->relay_address;
  dst->relay_port = src->relay_port;
  dst->last_connect_time = src->last_connect_time;
  dst->state = src->state;
  dst->type = src->type;
  dst->matched_address = src->matched_address;
  dst->matched_port = src->matched_port;
  dst->matched_role = src->matched_role;
  dst->capable_role = src->capable_role;
}

void CRouteTable::MainLoop(void* args) {
  while (m_bRun) {
    UINT64 current_time = 0;
    __OSAL_TIME_GetTimeMS((UINT64*)&current_time);

    __OSAL_Mutex_Lock(&m_accessKey);
    int count = m_LocalRoutingTable.GetCount();
    for (int i = 0; i < count; i++) {
      mapTable* ptt = m_LocalRoutingTable.GetAt(i);
      if ((current_time - ptt->last_connect_time) >= 60 * 1000) {
        m_LocalRoutingTable.DelAt(i);
        break;
      }
    }
    __OSAL_Mutex_UnLock(&m_accessKey);

    __OSAL_Mutex_Lock(&m_channelKey);
    count = m_LocalTurnTable.GetCount();
    for (int i = 0; i < count; i++) {
      turnTable* ptt = m_LocalTurnTable.GetAt(i);
      if ((current_time - ptt->last_connect_time) >= 60 * 1000) {
        m_LocalTurnTable.DelAt(i);
        break;
      }
    }
    __OSAL_Mutex_UnLock(&m_channelKey);

    __OSAL_Sleep(1000);  // 1 minute
  }
}
