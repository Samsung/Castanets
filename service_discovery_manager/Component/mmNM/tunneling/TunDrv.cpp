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

#include "Debugger.h"

#ifndef OTUNSETNOCSUM
#define OTUNSETIFF (('T' << 8) | 202)
#endif

CTunDrv::CTunDrv() {}

CTunDrv::~CTunDrv() {}

int CTunDrv::Open(char* dev, char* pb_addr) {
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
}

int CTunDrv::Close(int fd) {
  return close(fd);
}

#ifndef LEESS
int CTunDrv::Read(int fd, char* buf, int len) {
  int nReadByte = 0;
  int count = 10;

  while (--count) {
    nReadByte = read(fd, buf, len);

    if (nReadByte > 0)
      return nReadByte;

    usleep(1000 * 1000);
    printf("TunDrv Read Sleep(%d)\n", count);
  }

  return -1;
}

int CTunDrv::Write(int fd, char* buf, int len) {
  int nWriteByte = 0, total = 0;
  ;
  int count = 10;

  while (--count) {
    nWriteByte = write(fd, buf, len);

    total += nWriteByte;

    if (total >= len)
      return total;

    usleep(1000 * 1000);
    printf("TunDrv Write Sleep(%d)\n", count);
  }

  return -1;
}

#else

int CTunDrv::Read(int fd, char* buf, int len) {
  return read(fd, buf, len);
}

int CTunDrv::Write(int fd, char* buf, int len) {
  return write(fd, buf, len);
}
#endif
