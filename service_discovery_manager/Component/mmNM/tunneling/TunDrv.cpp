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

#include "TunDrv.h"


#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef WIN32
#include <syslog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#else
 #define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#endif

#include "Debugger.h"

#ifndef OTUNSETNOCSUM
#define OTUNSETIFF (('T' << 8) | 202)
#endif


#ifdef WIN32
#define _TAP_IOCTL(nr) CTL_CODE(FILE_DEVICE_UNKNOWN, nr, METHOD_BUFFERED, FILE_ANY_ACCESS)  
 
#define TAP_IOCTL_GET_MAC               _TAP_IOCTL(1) 
#define TAP_IOCTL_GET_VERSION           _TAP_IOCTL(2) 
#define TAP_IOCTL_GET_MTU               _TAP_IOCTL(3) 
#define TAP_IOCTL_GET_INFO              _TAP_IOCTL(4) 
#define TAP_IOCTL_CONFIG_POINT_TO_POINT _TAP_IOCTL(5) 
#define TAP_IOCTL_SET_MEDIA_STATUS      _TAP_IOCTL(6) 
#define TAP_IOCTL_CONFIG_DHCP_MASQ      _TAP_IOCTL(7) 
#define TAP_IOCTL_GET_LOG_LINE          _TAP_IOCTL(8) 
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT   _TAP_IOCTL(9) 
#define TAP_IOCTL_CONFIG_TUN            _TAP_IOCTL(10) 
#endif

CTunDrv::CTunDrv() {}

CTunDrv::~CTunDrv() {}

int CTunDrv::Open(char* dev, char* pb_addr) {

#ifndef WIN32
  struct ifreq ifr;
  int fd;
#ifdef ANDROID
  if ((fd = open("/dev/tun", O_RDWR | O_NONBLOCK)) < 0) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot Open Tunneling Driver [/dev/tun]\n");
    return -1;
  }
#else
  if ((fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot Open Tunneling Driver [/dev/net/tun]\n");
    return -1;
  }
#endif

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

  if (*dev) {
    DPRINT(COMM, DEBUG_ERROR, "cp ifname [%s]\n", dev);
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if (ioctl(fd, TUNSETIFF, (void*)&ifr) < 0) {
    if (errno == EBADFD) {
      /* Try old ioctl */
      if (ioctl(fd, OTUNSETIFF, (void*)&ifr) < 0) {
        DPRINT(COMM, DEBUG_ERROR, "Cannot Set Driver OTUNSETIFF\n");
        return -1;
      }
    } else {
      DPRINT(COMM, DEBUG_ERROR, "Cannot Set Driver OTUNSETIFF\n");
      return -1;
    }
  }
  strcpy(dev, ifr.ifr_name);
  DPRINT(COMM, DEBUG_ERROR, "Set IFF [%s]\n", dev);

  char cmd_buf[1024];

  memset(cmd_buf, 0, 1024);
  sprintf(cmd_buf, "ifconfig %s inet %s", ifr.ifr_name, pb_addr);
  DPRINT(COMM, DEBUG_INFO, "sh command : %s\n", cmd_buf);
  system(cmd_buf);

  memset(cmd_buf, 0, 1024);
  sprintf(cmd_buf, "route add -net 10.10.10.0 netmask 255.255.255.0 gw %s",
          pb_addr);
  DPRINT(COMM, DEBUG_INFO, "sh command : %s\n", cmd_buf);
  system(cmd_buf);

  /*
          for(int i=1;i<=255; i++)
          {
                  memset(cmd_buf,0,1024);
                  sprintf(cmd_buf,"route add 10.10.10.%d dev %s", i,
     ifr.ifr_name);
                  DPRINT(COMM,DEBUG_INFO,"sh command : %s\n",cmd_buf);
                  system(cmd_buf);
          }
  */

  // route add 10.10.10.3 tun0
  return fd;
#else
  return 0;
#endif
}

int CTunDrv::Close(int fd) {
#ifndef WIN32
  return close(fd);
#else
  return 0;
#endif
}

int CTunDrv::Read(int fd, char* buf, int len) {
#ifndef WIN32
  int nReadByte = 0;
  int count = 10;

  while (--count) {
    nReadByte = read(fd, buf, len);

    if (nReadByte > 0)
      return nReadByte;

    usleep(1000);
    printf("TunDrv Read Sleep(%d)\n", count);
  }

  return -1;
#else
  return -1;
#endif
}

int CTunDrv::Write(int fd, char* buf, int len) {
#ifndef WIN32
  int nWriteByte = 0, total = 0;
 
  int count = 10;

  while (--count) {
    nWriteByte = write(fd, buf, len);

    total += nWriteByte;

    if (total >= len)
      return total;

    usleep(1000);
    printf("TunDrv Write Sleep(%d)\n", count);
  }

  return -1;
#else
  return -1;
#endif
}

