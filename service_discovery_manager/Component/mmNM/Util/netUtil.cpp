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

#ifdef WIN32

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#endif

#ifdef WIN32

#else
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "netUtil.h"
#include "Debugger.h"


void U::SHOW_PACKET(const char* msg, unsigned char* buf, int len) {
  DPRINT(COMM, DEBUG_ALL, "%s\n", msg);
  DPRINT(BLNK, DEBUG_ALL, "\t=========================================\n\n");
  DPRINT(BLNK, DEBUG_ALL, "\t");
  for (int i = 1; i <= len; i++) {
    DPRINT(BLNK, DEBUG_ALL, "%6x", buf[i - 1]);
    if (!(i % 4)) {
      DPRINT(BLNK, DEBUG_ALL, "\n");
      DPRINT(BLNK, DEBUG_ALL, "\t");
    }
  }
  DPRINT(BLNK, DEBUG_ALL, "\n\t=========================================\n");
}

void U::SHOW_TABLE(CRouteTable::mapTable* t) {
  char src_addr[16];
  char map_addr[16];
  char rel_addr[16];

  memset(src_addr, 0, 16);
  memset(map_addr, 0, 16);
  memset(rel_addr, 0, 16);

  sprintf(src_addr, "%ld.%ld.%ld.%ld", (t->source_address & 0x000000FF),
          (t->source_address & 0x0000FF00) >> 8,
          (t->source_address & 0x00FF0000) >> 16,
          (t->source_address & 0xFF000000) >> 24);
  sprintf(map_addr, "%ld.%ld.%ld.%ld", (t->mapped_address & 0x000000FF),
          (t->mapped_address & 0x0000FF00) >> 8,
          (t->mapped_address & 0x00FF0000) >> 16,
          (t->mapped_address & 0xFF000000) >> 24);
  sprintf(rel_addr, "%ld.%ld.%ld.%ld", (t->relay_address & 0x000000FF),
          (t->relay_address & 0x0000FF00) >> 8,
          (t->relay_address & 0x00FF0000) >> 16,
          (t->relay_address & 0xFF000000) >> 24);

  DPRINT(COMM, DEBUG_INFO,
         "TABLE => SOURCE[%s:%d] MAPPED[%s:%d] RELAY[%s:%d]\n", src_addr,
         t->source_port, map_addr, t->mapped_port, rel_addr, t->relay_port);
}

char* U::GET_TABLE(CRouteTable::mapTable* t, int type) {

  char* addr = new char[22]; //including port number(':' + 5 digits as max)

  memset(addr, 0, 22);

  if (type == 0) // source addr
    sprintf(addr, "%ld.%ld.%ld.%ld:%d",
            (t->source_address & 0x000000FF),
            (t->source_address & 0x0000FF00) >> 8,
            (t->source_address & 0x00FF0000) >> 16,
            (t->source_address & 0xFF000000) >> 24,
            t->source_port);
  else if (type == 1) // mapped addr
    sprintf(addr, "%ld.%ld.%ld.%ld:%d",
            (t->mapped_address & 0x000000FF),
            (t->mapped_address & 0x0000FF00) >> 8,
            (t->mapped_address & 0x00FF0000) >> 16,
            (t->mapped_address & 0xFF000000) >> 24,
            t->mapped_port);
  else if (type == 2) // matched addr
    sprintf(addr, "%ld.%ld.%ld.%ld:%d",
            (t->matched_address & 0x000000FF),
            (t->matched_address & 0x0000FF00) >> 8,
            (t->matched_address & 0x00FF0000) >> 16,
            (t->matched_address & 0xFF000000) >> 24,
            t->matched_port);
  else if (type == 3) // matched role
    sprintf(addr, "%s", (t->matched_role == CRouteTable::BROWSER) ?
           "BROWSER" : (t->matched_role == CRouteTable::RENDERER) ?
           "RENDERER" : "NONE");
  return addr;
}

void U::SHOW_ADDR(const char* which, unsigned long addr, unsigned short port) {
  char src_addr[16];
  memset(src_addr, 0, 16);
  sprintf(src_addr, "%ld.%ld.%ld.%ld", (addr & 0x000000FF),
          (addr & 0x0000FF00) >> 8, (addr & 0x00FF0000) >> 16,
          (addr & 0xFF000000) >> 24);
  DPRINT(COMM, DEBUG_INFO, "ADDR => %s[%s:%d]\n", which, src_addr, port);
}

char* U::CONV(unsigned long addr) {
  char* pszAddr = new char[16];
  memset(pszAddr, 0, 16);
  sprintf(pszAddr, "%ld.%ld.%ld.%ld", (addr & 0x000000FF),
          (addr & 0x0000FF00) >> 8, (addr & 0x00FF0000) >> 16,
          (addr & 0xFF000000) >> 24);
  return pszAddr;
}

unsigned long U::CONV(char* addr) {
  return 0;
}

/*
static char u_sz_addr[16];
void U::u_dumpPacket(const char* msg, unsigned char* buf, int len)
{
        DPRINT(COMM,DEBUG_ALL,"%s\n",msg);
        RAW_PRINT("\t=========================================\n\n");
        RAW_PRINT("\t");
        for(int i=1;i<=len;i++)
        {
                RAW_PRINT("%6x",buf[i-1]);
                if(!(i%4))
                {
                                RAW_PRINT("\n");
                                RAW_PRINT("\t");
                }
        }
        RAW_PRINT("\n\t=========================================\n");

}

char* U::u_getStrAddress(unsigned long addr)
{
        memset(u_sz_addr,0,16);
        sprintf(u_sz_addr,"%ld.%ld.%ld.%ld",(addr&0x000000FF),(addr&0x0000FF00)>>8,(addr&0x00FF0000)>>16,(addr&0xFF000000)>>24);
        return u_sz_addr;
}

char* U::u_allocStrAddress(unsigned long addr)
{
        char* pszAddr=new char[16];
        memset(pszAddr,0,16);
        sprintf(pszAddr,"%ld.%ld.%ld.%ld",(addr&0x000000FF),(addr&0x0000FF00)>>8,(addr&0x00FF0000)>>16,(addr&0xFF000000)>>24);
        return pszAddr;
}


unsigned long U::u_getDexAddress(char* addr)
{
        return 0;
}
*/
