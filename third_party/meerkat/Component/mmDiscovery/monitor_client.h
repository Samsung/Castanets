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

#ifndef __INCLUDE_MONITOR_CLIENT_H__
#define __INCLUDE_MONITOR_CLIENT_H__

#include "bGlobDef.h"
#include "pTcpClient.h"

#include <string>

#define INVALID_RTT -1

typedef struct MonitorInfo_ {
  std::string id;
  double rtt;
  float cpu_usage;
  int cpu_cores;
  float frequency;
  double bandwidth;
} MonitorInfo;

class MonitorClient;
class ClientSocket : public mmProto::CpTcpClient {
 public:
  ClientSocket(MonitorClient* parent);
  ClientSocket(MonitorClient* parent, const CHAR* id);

  VOID DataRecv(OSAL_Socket_Handle sock,
                const CHAR* addr,
                long port,
                CHAR* data,
                INT32 len);
  VOID EventNotify(CbSocket::SOCKET_NOTIFYTYPE type);
  bool GenerateInfo(CHAR* data);

 private:
  MonitorClient* parent_;
  MonitorInfo info_;
};

class MonitorClient {
 public:
  MonitorClient();
  MonitorClient(const CHAR* id);
  virtual ~MonitorClient();

  BOOL Start(const CHAR* address, int port, int read = -1);
  BOOL Stop();
  VOID DataSend(CHAR* data, int len);

  double Rtt() { return rtt_; }
  VOID CheckRtt();
  VOID ParseRtt();

 private:
  ClientSocket sock_;
  std::string ping_;
  double rtt_;
};

#endif  // __INCLUDE_MONITOR_CLIENT_H__
