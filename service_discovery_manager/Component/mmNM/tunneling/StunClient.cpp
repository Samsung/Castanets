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

#include "StunClient.h"

#ifndef WIN32
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/if.h>
#endif

#include "Debugger.h"

using namespace mmBase;

#define SET_CHAR(p, v) \
  if (p) {             \
    *(p) = v;          \
  }

#define SET_SHORT(p, v)                                                       \
  if (p) {                                                                    \
    *((unsigned char*)p) =                                                    \
        (unsigned char)((((unsigned short)v) & 0xFF00) >> 8);                 \
    *((unsigned char*)p + 1) = (unsigned char)(((unsigned short)v) & 0x00FF); \
  }

#define SET_LONG(p, v)                                            \
  if (p) {                                                        \
    *((unsigned char*)p) =                                        \
        (unsigned char)((((unsigned long)v) & 0xFF000000) >> 24); \
    *((unsigned char*)p + 1) =                                    \
        (unsigned char)((((unsigned long)v) & 0x00FF0000) >> 16); \
    *((unsigned char*)p + 2) =                                    \
        (unsigned char)((((unsigned long)v) & 0x0000FF00) >> 8);  \
    *((unsigned char*)p + 3) =                                    \
        (unsigned char)(((unsigned long)v) & 0x000000FF);         \
  }

/*
static inline void SET_SHORT(char* p, unsigned short v)
{
        if(!
}
*/

static inline unsigned long GET_LONG(char* p) {
  unsigned long l = 0;
  if (p) {
    l = (unsigned long)(((*p) << 24) & 0xFF000000) |
        (((*(p + 1)) << 16) & 0x00FF0000) | (((*(p + 2)) << 8) & 0x0000FF00) |
        ((*(p + 3)) & 0x000000FF);
  }
  return l;
}

static inline unsigned short GET_SHORT(char* p) {
  unsigned short s = 0;
  if (p) {
    s = (unsigned short)(((*p) << 8) & 0xFF00) | ((*(p + 1)) & 0x00FF);
  }
  return s;
}

static inline unsigned short GET_CHAR(char* p) {
  unsigned char c = 0;
  if (p) {
    c = (unsigned char)*p;
  }
  return c;
}

CStunClient::CStunClient() {}

CStunClient::~CStunClient() {}

int CStunClient::bpRequest(char* buf,
                           STUN_MSG_TYPE type,
                           unsigned long src_addr,
                           unsigned short src_port,
                           unsigned long mapped_addr,
                           unsigned short mapped_port) {
  unsigned char msg_hdr[STUN_MESSAGE_HDR_LEN];
  unsigned char msg_attr[4];
  unsigned char con_info[8];

  memset(msg_hdr, 0, STUN_MESSAGE_HDR_LEN);
  memset(msg_attr, 0, 4);
  memset(con_info, 0, 8);

  SET_SHORT(&msg_hdr[0], (unsigned short)type);
  SET_SHORT(&msg_hdr[2], (unsigned short)24);

  memcpy(buf, msg_hdr, STUN_MESSAGE_HDR_LEN);

  // fill transaction id

  SET_SHORT(&msg_attr[0], (unsigned short)SOURCE_ADDRESS);
  SET_SHORT(&msg_attr[2], (unsigned short)8);
  SET_SHORT(&con_info[0], (unsigned short)0x1);
  SET_SHORT(&con_info[2], (unsigned short)src_port);
  SET_LONG(&con_info[4], (unsigned long)src_addr);
  memcpy(buf + STUN_MESSAGE_HDR_LEN, msg_attr, STUN_ATTRIBUTE_HDR_LEN);
  memcpy(buf + STUN_MESSAGE_HDR_LEN + STUN_ATTRIBUTE_HDR_LEN, con_info,
         STUN_ADDRINFO_HDR_LEN);

  SET_SHORT(&msg_attr[0], (unsigned short)MAPPED_ADDRESS);
  SET_SHORT(&msg_attr[2], (unsigned short)8);
  SET_SHORT(&con_info[0], (unsigned short)0x1);
  SET_SHORT(&con_info[2], (unsigned short)mapped_port);
  SET_LONG(&con_info[4], (unsigned long)mapped_addr);
  memcpy(buf + STUN_MESSAGE_HDR_LEN + STUN_ATTRIBUTE_HDR_LEN +
             STUN_ADDRINFO_HDR_LEN,
         msg_attr, STUN_ATTRIBUTE_HDR_LEN);
  memcpy(buf + STUN_MESSAGE_HDR_LEN + STUN_ATTRIBUTE_HDR_LEN +
             STUN_ADDRINFO_HDR_LEN + STUN_ATTRIBUTE_HDR_LEN,
         con_info, STUN_ADDRINFO_HDR_LEN);

  return (STUN_MESSAGE_HDR_LEN +
          2 * (STUN_ATTRIBUTE_HDR_LEN + STUN_ADDRINFO_HDR_LEN));
}

/*
int CStunClient::bpRequest(unsigned char* buf, STUN_MSG_TYPE type, unsigned long
src_addr, unsigned short src_port)
{
        unsigned char msg_hdr[STUN_MESSAGE_HDR_LEN];
        unsigned char msg_attr[4];
        unsigned char con_info[8];

        memset(msg_hdr,0,STUN_MESSAGE_HDR_LEN);
        memset(msg_attr,0,4);
        memset(msg_attr,0,8);

        SET_SHORT(&msg_hdr[0],(unsigned short)type);

        SET_SHORT(&msg_hdr[2],(unsigned short)12);

        //fill transaction id

        SET_SHORT(&msg_attr[0],(unsigned short)SOURCE_ADDRESS);
        SET_SHORT(&msg_attr[2],(unsigned short)8);



        SET_SHORT(&con_info[0],(unsigned short)0x1);
        SET_SHORT(&con_info[2],(unsigned short)src_port);


        //unsigned long ui_addr = 0;
        //inet_pton(AF_INET, src_addr, (void *)&ui_addr);

        SET_LONG(&con_info[4],src_addr);
        memcpy(buf, msg_hdr,STUN_MESSAGE_HDR_LEN);
        memcpy(buf+STUN_MESSAGE_HDR_LEN, msg_attr,STUN_ATTRIBUTE_HDR_LEN);
        memcpy(buf+STUN_MESSAGE_HDR_LEN+STUN_ATTRIBUTE_HDR_LEN,
con_info,STUN_ADDRINFO_HDR_LEN);

        return
STUN_MESSAGE_HDR_LEN+STUN_ATTRIBUTE_HDR_LEN+STUN_ADDRINFO_HDR_LEN;
}
*/

int CStunClient::cpRequest(char* buf,
                           STUN_MSG_TYPE* pType,
                           CbList<stun_msg_attr>* pAttributeList,
                           int t_len) {
  unsigned short buf_len = GET_SHORT(&buf[2]);
  unsigned short msg_type = GET_SHORT(&buf[0]);
  *pType = (STUN_MSG_TYPE)msg_type;

  if (t_len < STUN_MESSAGE_HDR_LEN + STUN_ATTRIBUTE_HDR_LEN) {
    DPRINT(COMM, DEBUG_INFO, "cpRequest ==> Message Length too short:%d\n",
           t_len);
    return -1;
  }

  if ((*pType != BINDING_REQUEST) && (*pType != SHARED_SECRET_REQUEST) &&
      (*pType != DHCP_REQUEST) && (*pType != MAPQUERY_REQUEST) &&
      (*pType != TRIAL_REQUEST) && (*pType != TURNALLOC_REQUEST) &&
      (*pType != TARGETB_REQUEST) && (*pType != TARGETR_REQUEST) &&
      (*pType != SELECTION_UPDATE_REQUEST)) {
    DPRINT(COMM, DEBUG_INFO, "cpRequest ==> unknown messge type 0x%x\n",
           *pType);
    return -1;
  }

  if (t_len < buf_len)
    return -1;

  long index = STUN_MESSAGE_HDR_LEN;

  while (true) {
    unsigned short attr = GET_SHORT(&buf[index]);
    unsigned short data_len = GET_SHORT(&buf[index + 2]);

    if ((attr == 0x0) || attr > 0xb)
      return -1;

    if (index + data_len > t_len)
      return -1;

    char* pszData = new char[data_len + 1];
    memset(pszData, 0, data_len + 1);
    memcpy(pszData, &buf[index + STUN_ATTRIBUTE_HDR_LEN], data_len);

    stun_msg_attr* pAttr = new stun_msg_attr;
    pAttr->type = (STUN_MSG_ATTRIBUTE)attr;
    pAttr->length = data_len;
    pAttr->value = pszData;

    pAttributeList->AddTail(pAttr);
    index += STUN_ATTRIBUTE_HDR_LEN + data_len;
    if (index >= buf_len + STUN_MESSAGE_HDR_LEN)
      break;
  }

  return buf_len;
}

int CStunClient::cpResponse(char* buf,
                            STUN_MSG_TYPE* pType,
                            CbList<stun_msg_attr>* pAttributeList,
                            int t_len) {
  unsigned short buf_len = GET_SHORT(&buf[2]);
  unsigned short msg_type = GET_SHORT(&buf[0]);
  *pType = (STUN_MSG_TYPE)msg_type;

  if (t_len < STUN_MESSAGE_HDR_LEN + STUN_ATTRIBUTE_HDR_LEN) {
    DPRINT(COMM, DEBUG_INFO, "cpResponse ==> Message Length too short:%d\n",
           t_len);
    return -1;
  }
  if ((*pType != BINDING_RESPONSE) && (*pType != BINDING_ERROR_RESPONSE) &&
      (*pType != SHARED_SECRET_RESPONSE) &&
      (*pType != SHARED_SECRET_ERROR_RESPONSE) && (*pType != DHCP_RESPONSE) &&
      (*pType != DHCP_ERROR_RESPONSE) && (*pType != MAPQUERY_RESPONSE) &&
      (*pType != MAPQUERY_ERROR_RESPONSE) && (*pType != TRIAL_ERROR_RESPONSE) &&
      (*pType != TRIAL_RESPONSE) && (*pType != TURNALLOC_ERROR_RESPONSE) &&
      (*pType != TURNALLOC_RESPONSE) && (*pType != TARGETB_ERROR_RESPONSE) &&
      (*pType != TARGETB_RESPONSE) && (*pType != TARGETR_ERROR_RESPONSE) &&
      (*pType != TARGETR_RESPONSE) && (*pType != SELECTION_UPDATE_ERROR_RESPONSE) &&
      (*pType != SELECTION_UPDATE_RESPONSE)) {
    DPRINT(COMM, DEBUG_INFO, "cpResponse ==> unknown messge type 0x%x\n",
           *pType);
    return -1;
  }
  if (t_len < buf_len) {
    DPRINT(
        COMM, DEBUG_INFO,
        "cpResponse ==> Header info(msglen) mismached (hdr:%d) (physical:%d)\n",
        buf_len, t_len);
    return -1;
  }
  long index = STUN_MESSAGE_HDR_LEN;

  while (true) {
    unsigned short attr = GET_SHORT(&buf[index]);
    unsigned short data_len = GET_SHORT(&buf[index + 2]);

    if ((attr == 0x0) || attr > 0xb)
      return -1;

    if (index + data_len > t_len)
      return -1;

    char* pszData = new char[data_len + 1];
    memset(pszData, 0, data_len + 1);
    memcpy(pszData, &buf[index + STUN_ATTRIBUTE_HDR_LEN], data_len);

    stun_msg_attr* pAttr = new stun_msg_attr;
    pAttr->type = (STUN_MSG_ATTRIBUTE)attr;
    pAttr->length = data_len;
    pAttr->value = pszData;

    pAttributeList->AddTail(pAttr);
    index += STUN_ATTRIBUTE_HDR_LEN + data_len;
    if (index >= buf_len + STUN_MESSAGE_HDR_LEN)
      break;
  }

  return buf_len;
}

int CStunClient::cpAddress(char* buf, stun_addr_info* addr_info) {
  addr_info->family = GET_CHAR(&buf[1]);
  addr_info->port = GET_SHORT(&buf[2]);
  addr_info->Address = GET_LONG(&buf[4]);
  return TRUE;
}
