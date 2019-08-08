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

#ifndef __INCLUDE_TUN_SERVER_H__
#define __INCLUDE_TUN_SERVER_H__


#include "TunDrv.h"
#include "posixAPI.h"
#include "bThread.h"

typedef void (*pfTunHandler)(int type, int len, void* data);

class CTunServer : public mmBase::CbThread {
 public:
  struct s_tun {
    int r_fd;
    int l_fd;
  };

 public:
  CTunServer(const char* pszTaskName);
  virtual ~CTunServer();

  BOOL TunnelingStart(pfTunHandler handler,
                      char* pb_addr,
                      INT32 read_per_once = 1024);
  BOOL TunnelingStop();

  virtual VOID EventNotify(int iEventSock, int type);
  virtual VOID DataRecv(int iEventSock, CHAR* pData, INT32 iLen);

  virtual INT32 DataSend(CHAR* pData, INT32 iLen);

 private:
  VOID MainLoop(void* args);

 private:
  INT32 m_read_per_once;
  CTunDrv* m_pTunDriver;
  s_tun* m_pTunInfo;
  pfTunHandler m_handler;
  BOOL m_bTunEnable;
  OSAL_Mutex_Handle m_hMutex;
};

#endif  //__INCLUDE_TUN_SERVER_H__
