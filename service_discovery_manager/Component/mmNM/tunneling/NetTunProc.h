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

#ifndef __INCLUDE_NET_TUN_PROC_H__
#define __INCLUDE_NET_TUN_PROC_H__

//#include "bTask.h"
#include "RmtServer.h"
#include "TunServer.h"
#include "RouteTable.h"
#include "StunClient.h"

class CNetTunProc : public mmBase::CbTask {
 public:
  enum connection_type { DIRECT_STUN_CONN = 0, RELAYED_TURM_CONN };

  struct init_argument {
    char server_ip[16];
    unsigned short tun_port;
    int read_once;
    int time_unit;
    int bind_period;
    int retry_count;
    CRouteTable::role_type role;
  };

  struct tun_addr_info {
    unsigned long source_address;
    unsigned short source_port;
    unsigned long mapped_address;
    unsigned long mapped_port;
  };

  struct csignal {
    OSAL_Mutex_Handle hMutex;
    OSAL_Event_Handle hEvent;
  };

 public:
  CNetTunProc(const char* pszTaskName,
              char* server_ip,
              unsigned short tun_port,
              int read_once,
              int time_unit,
              int bind_period,
              int retry_count);
  virtual ~CNetTunProc();
  BOOL Create();
  BOOL Destroy();

  BOOL ProcessRemotePacket(
      char* addr,
      int port,
      CStunClient::STUN_MSG_TYPE Type,
      mmBase::CbList<CStunClient::stun_msg_attr>* attrList);
  BOOL ProcessTunPacket(int tun_msg_type,
                        int tun_pkt_len,
                        unsigned char* frame);
  VOID DUMP_TABLE() { m_pTableHandler->DUMP_T(); }
  VOID DUMP_CHANNEL() { m_pTableHandler->DUMP_C(); }
  BOOL HasTarget() { return m_hasTarget; }
  unsigned long GetTarget();
  VOID SetRole(CRouteTable::role_type role) { m_args.role = role; }

 private:
  VOID t_CreateEvent();
  VOID t_DestroyEvent();
  BOOL t_ProcessDHCP();

  BOOL t_WaitForReply() { return TRUE; }

  static void OnRemoteMessage(int rmt_msg_type,
                              char* addr,
                              int port,
                              int rmt_pkt_len,
                              void* pData);

  static void OnLocalMessage(int type, int len, void* pData);

  void MainLoop(void* args);

 private:
  static CNetTunProc* m_hNetTunProc;
  tun_addr_info m_device_address;
  tun_addr_info m_target_address;
  BOOL m_hasTarget;

 protected:
  CRmtServer* m_pRemoteServer;
  CTunServer* m_pTunServer;
  CRouteTable* m_pTableHandler;

  csignal m_sigInit;
  csignal m_sigQuery;
  csignal m_sigAlive;
  csignal m_sigTrial;
  csignal m_sigTurnAlloc;
  csignal m_sigTarget;
  csignal m_sigSelectionUpdate;

  init_argument m_args;
};

#endif  //__INCLUDE_NET_TUN_PROC_H__
