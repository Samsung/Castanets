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

#ifndef __INCLUDE_NET_UTIL_H__
#define __INCLUDE_NET_UTIL_H__

#include "RouteTable.h"

class U {
 public:
  U() {}
  virtual ~U() {}

 public:  // public method
  static unsigned long CONV(char* addr);
  static char* CONV(unsigned long addr);
  static void SHOW_TABLE(CRouteTable::mapTable* t);
  static char* GET_TABLE(CRouteTable::mapTable* t, int type);
  static void SHOW_ADDR(const char* which,
                        unsigned long addr,
                        unsigned short port);
  static void SHOW_PACKET(const char* msg, unsigned char* buf, int len);

  /*
          static void u_dumpPacket(const char* msg, unsigned char* buf, int
     len);
          static char* u_getStrAddress(unsigned long addr);
          static char* u_allocStrAddress(unsigned long addr);
          static unsigned long u_getDexAddress(char* addr);
  */
 private:  // private method
};

#endif  //__INCLUDE_NET_UTIL_H__
