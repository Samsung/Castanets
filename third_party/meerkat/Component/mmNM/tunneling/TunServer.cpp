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

#include "TunServer.h"

#ifndef WIN32
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#include "netUtil.h"

#define VTUN_DEV_LEN 20
using namespace mmBase;

CTunServer::CTunServer(const char* pszTaskName) : CbThread(pszTaskName) {
  m_bTunEnable = FALSE;
  m_pTunDriver = new CTunDrv;
  CHECK_ALLOC(m_pTunDriver);

  m_pTunInfo = new CTunServer::s_tun;
  CHECK_ALLOC(m_pTunInfo);

  m_hMutex = __OSAL_Mutex_Create();
}

CTunServer::~CTunServer() {
  SAFE_DELETE(m_pTunDriver);
  SAFE_DELETE(m_pTunInfo);
  __OSAL_Mutex_Destroy(&m_hMutex);
}

BOOL CTunServer::TunnelingStart(pfTunHandler handler,
                                char* pb_addr,
                                INT32 read_per_once) {
  char dev[VTUN_DEV_LEN] = "";
  dev[VTUN_DEV_LEN - 1] = '\0';

  m_handler = handler;
  if ((m_pTunInfo->l_fd = m_pTunDriver->Open(dev, pb_addr)) < 0) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot Open tun driver\n");
    return FALSE;
  }
  m_bTunEnable = TRUE;
  m_read_per_once = read_per_once;

#ifndef LEESS
  FILE* fd;
  if ((fd = fopen("tunip.txt", "w")) == NULL) {
    printf("File open error.\n");
    return FALSE;
  }

  printf("File open success.\n");
  printf("Data: %s\n", pb_addr);

  UINT32 ip_len = strlen(pb_addr);
  UINT32 size = fwrite(pb_addr, sizeof(char), ip_len, fd);

  if (size != ip_len) {
    printf("File write error.\n");
    return FALSE;
  }

  printf("File write success.\n");

  fclose(fd);
#endif

  StartMainLoop((void*)m_pTunInfo);
  return TRUE;
}

BOOL CTunServer::TunnelingStop() {
  m_pTunDriver->Close(m_pTunInfo->l_fd);

  return TRUE;
}

VOID CTunServer::EventNotify(int iEventSock, int type) {}

VOID CTunServer::DataRecv(int iEventSock, CHAR* pData, INT32 iLen) {
  m_handler(0, iLen, pData);
}

INT32 CTunServer::DataSend(CHAR* pData, INT32 iLen) {
  __OSAL_Mutex_Lock(&m_hMutex);
  if (m_bTunEnable) {
    int iRet = m_pTunDriver->Write(m_pTunInfo->l_fd, pData, iLen);
    __OSAL_Mutex_UnLock(&m_hMutex);
    return iRet;
  } else {
    DPRINT(COMM, DEBUG_INFO, "### TUN is Not Ready !!!!! ###\n");
    __OSAL_Mutex_UnLock(&m_hMutex);
    return -1;
  }
}

VOID CTunServer::MainLoop(void* args) {
#ifndef WIN32
  int maxfd;
  struct timeval tv;
  fd_set fdset;
  int len;

  CTunServer::s_tun* ptun = (CTunServer::s_tun*)args;

  DPRINT(COMM, DEBUG_INFO, " Start Tunneling Loop with : [%d] !!!\n",
         ptun->l_fd);
  while (m_bRun) {
    FD_ZERO(&fdset);
    FD_SET(ptun->l_fd, &fdset);
    maxfd = ptun->l_fd + 1;

    tv.tv_sec = 10; /*5 second time out*/
    tv.tv_usec = 0;
    if ((len = select(maxfd, &fdset, NULL, NULL, &tv)) < 0) {
      if (errno != EAGAIN && errno != EINTR)
        break;
      else
        continue;
    }
    if (FD_ISSET(ptun->l_fd, &fdset)) {
      char* buf = (char*)malloc(m_read_per_once + 1);
      CHECK_ALLOC(buf);
      memset(buf, 0, m_read_per_once + 1);
      __OSAL_Mutex_Lock(&m_hMutex);
      len = m_pTunDriver->Read(ptun->l_fd, buf, m_read_per_once);
      __OSAL_Mutex_UnLock(&m_hMutex);
      if (len <= 0) {
        DPRINT(COMM, DEBUG_WARN, "Dev Read Fail\n");
        SAFE_FREE(buf);
        __OSAL_Mutex_UnLock(&m_hMutex);
        continue;
      }
      DataRecv(ptun->l_fd, buf, len);
      SAFE_FREE(buf);
    }
  }
#else
	while (m_bRun) {
		__OSAL_Sleep(100);
	}
#endif
  return;
}
