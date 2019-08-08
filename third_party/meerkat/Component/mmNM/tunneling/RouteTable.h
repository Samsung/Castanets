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

#ifndef __INCLUDE_ROUTE_TABLE_H__
#define __INCLUDE_ROUTE_TABLE_H__

//

#include "osal.h"
#include "bList.h"
#include "bThread.h"
#include "bDataType.h"

class CRouteTable : public mmBase::CbThread {
 public:
  enum querykey_type {
    QUERY_BY_SOURCEADDR = 0,
    QUERY_BY_MAPPEDADDR,
  };

  enum connection_type {
    CONN_NOT_ESTABLISHED = 0,
    DIRECT_STUN_CONN,
    RELAYED_TURN_CONN
  };

  enum channel_state { LOCKED_TRUN_CHANNEL = 0, UNLOCK_TURN_CHANNEL };

  enum role_type {
    NONE = 0,
    BROWSER,
    RENDERER,
    BOTH
  };

  struct mapTable {
    unsigned long source_address = 0;
    unsigned long mapped_address = 0;
    unsigned long relay_address = 0;
    unsigned long matched_address = 0;
    unsigned short source_port = 0;
    unsigned short mapped_port = 0;
    unsigned short relay_port = 0;
    unsigned short matched_port = 0;

    connection_type type = CONN_NOT_ESTABLISHED;
    channel_state state = LOCKED_TRUN_CHANNEL;
    role_type matched_role = NONE;
    role_type capable_role = NONE;

    UINT64 last_connect_time = 0;
  };

  struct turnTable {
    unsigned long endpoint[2];
    unsigned long relaypoint;
    UINT64 last_connect_time;
  };

 public:
  CRouteTable(const char* pszName);
  virtual ~CRouteTable();

 public:  // public method
  BOOL RouteTableCheckerStart();
  BOOL RouteTableCheckerStop();

  // route table management API
  BOOL AddPath(mapTable* pTable);
  BOOL DelPath(unsigned long source_address, unsigned short source_port);
  BOOL Access(unsigned long address, unsigned short port);

  mapTable* QueryTable(unsigned long address,
                       unsigned short port,
                       querykey_type type);
  mapTable* QueryTarget(unsigned long address, role_type role);
  BOOL UpdateTable(unsigned long sourceaddr, unsigned short port, mapTable* p);
  BOOL SetConnType(unsigned long address,
                   unsigned short port,
                   connection_type type);
  BOOL SetChannelState(unsigned long address,
                       unsigned short port,
                       channel_state state);

  // turn channel management API

  BOOL DelChannel(turnTable* pTable);
  BOOL AddChannel(turnTable* pTable);
  BOOL Access(unsigned long ep0, unsigned long ep1);
  turnTable* QueryChannel(unsigned long ep0, unsigned long ep1);

  VOID DUMP_T();
  INT32 MEMDUMP_T(PCHAR bucket[]);
  VOID DUMP_C();

 private:
  void CopyTable(mapTable* src, mapTable* dst);
  void MainLoop(void* args);

 private:  // private method
  mmBase::CbList<mapTable> m_LocalRoutingTable;
  mmBase::CbList<turnTable> m_LocalTurnTable;

  OSAL_Mutex_Handle m_accessKey;
  OSAL_Mutex_Handle m_channelKey;
};

#endif  //__INCLUDE_ROUTE_TABLE_H__
