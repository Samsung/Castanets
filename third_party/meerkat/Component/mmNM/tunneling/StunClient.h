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

#ifndef __INCLUDE_STUN_CLIENT_H__
#define __INCLUDE_STUN_CLIENT_H__

#include "bDataType.h"
#include "bList.h"

#define STUN_MESSAGE_HDR_LEN 20
#define STUN_ATTRIBUTE_HDR_LEN 4
#define STUN_ADDRINFO_HDR_LEN 8

class CStunClient {
 public:
  enum STUN_MSG_TYPE {
    BINDING_REQUEST = 0x0001,
    BINDING_RESPONSE = 0x0101,
    BINDING_ERROR_RESPONSE = 0x0111,
    SHARED_SECRET_REQUEST = 0x0002,
    SHARED_SECRET_RESPONSE = 0x0102,
    SHARED_SECRET_ERROR_RESPONSE = 0x0112,
    // EXTENSION
    DHCP_REQUEST = 0x0003,
    DHCP_RESPONSE = 0x0103,
    DHCP_ERROR_RESPONSE = 0x0113,

    MAPQUERY_REQUEST = 0x0004,
    MAPQUERY_RESPONSE = 0x0104,
    MAPQUERY_ERROR_RESPONSE = 0x0114,

    TRIAL_REQUEST = 0x0005,
    TRIAL_RESPONSE = 0x0105,
    TRIAL_ERROR_RESPONSE = 0x0115,

    TURNALLOC_REQUEST = 0x0006,
    TURNALLOC_RESPONSE = 0x0106,
    TURNALLOC_ERROR_RESPONSE = 0x0116,

    TARGETB_REQUEST = 0x0007,
    TARGETB_RESPONSE = 0x0107,
    TARGETB_ERROR_RESPONSE = 0x0117,

    TARGETR_REQUEST = 0x0008,
    TARGETR_RESPONSE = 0x0108,
    TARGETR_ERROR_RESPONSE = 0x0118,

    SELECTION_UPDATE_REQUEST = 0x0009,
    SELECTION_UPDATE_RESPONSE = 0x0109,
    SELECTION_UPDATE_ERROR_RESPONSE = 0x0119,
  };

  enum STUN_MSG_ATTRIBUTE {
    MAPPED_ADDRESS = 0x0001,
    RESPONSE_ADDRESS = 0x0002,
    CHANGE_REQUEST = 0x0003,
    SOURCE_ADDRESS = 0x0004,
    CHANGED_ADDRESS = 0x0005,
    USERNAME = 0x0006,
    PASSWORD = 0x0007,
    MESSAGE_INTEGRITY = 0x0008,
    ERROR_CODE = 0x0009,
    UNKNOWN_ATTRIBUTES = 0x000a,
    REFLECTED_FROM = 0x000b
  };

  /*
           0					 1					 2
     3
           0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |	   STUN Message Type		|		  Message Length
     |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                           Transaction ID
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                                                                                          |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */

  struct stun_msg_type {
    unsigned short type;
    unsigned short length;
    unsigned long id[4];
  };

  /*
          Message Attributes
          0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             |		 Type				   |			Length
     |
             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             |							 Value
     ....
             +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */
  struct stun_msg_attr {
    unsigned short type;
    unsigned short length;
    char* value;
  };

  /*
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |x x x x x x x x|    Family     |           Port                |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                             Address                           |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */
  struct stun_addr_info {
    unsigned short family;
    unsigned short port;
    unsigned long Address;
  };

 public:
  CStunClient();
  virtual ~CStunClient();

 public:  // public method
  // static int bpRequest(unsigned char* buf, STUN_MSG_TYPE type, unsigned long
  // src_addr, unsigned short src_port);
  static int bpRequest(char* buf,
                       STUN_MSG_TYPE type,
                       unsigned long src_addr,
                       unsigned short src_port,
                       unsigned long mapped_addr = 0,
                       unsigned short mapped_port = 0);
  static int cpRequest(char* buf,
                       STUN_MSG_TYPE* pType,
                       mmBase::CbList<stun_msg_attr>* pAttributeList,
                       int t_len);
  static int cpResponse(char* buf,
                        STUN_MSG_TYPE* pType,
                        mmBase::CbList<stun_msg_attr>* pAttributeList,
                        int t_len);
  static int cpAddress(char* buf, stun_addr_info* addr_info);

 private:  // private method
};

#endif  //__INCLUDE_STUN_CLIENT_H__
