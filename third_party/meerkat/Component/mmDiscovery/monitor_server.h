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

#ifndef __INCLUDE_MONITOR_SERVER_H__
#define __INCLUDE_MONITOR_SERVER_H__

#include "pTcpServer.h"

#include <list>
#include <string>

#define SERVER_MONITORING_TIME 1000
#define MAX_MONITOR_MSG_BUFF 512

class MonitorServer;
class ServerSocket : public mmProto::CpTcpServer {
 public:
  ServerSocket(MonitorServer* parent);
  ServerSocket(MonitorServer* parent, const CHAR* msg_name);

  VOID DataRecv(OSAL_Socket_Handle sock,
                const CHAR* addr,
                long port,
                CHAR* data,
                INT32 len);
  VOID EventNotify(OSAL_Socket_Handle sock,
                   mmBase::CbSocket::SOCKET_NOTIFYTYPE type);
  bool MakeMonitiorInfo();

 private:
  MonitorServer* parent_;
  std::string monitor_info_;
};

class MonitorThread : public mmBase::CbThread {
 public:
  MonitorThread(MonitorServer* parent);
  MonitorThread(MonitorServer* parent, const CHAR* name);
  void MainLoop(void* args);

 private:
  void CheckBandwidth();
  void CheckMemoryUsage();
  void CheckCpuUsage();

  MonitorServer* parent_;
};

class MonitorServer /*: public CpTcpServer*/ {
 public:
  MonitorServer();
  MonitorServer(const CHAR* msgqname);
  virtual ~MonitorServer();

  BOOL Start(int port, int read = -1);
  BOOL Stop();
  VOID DataRecv(OSAL_Socket_Handle sock,
                const CHAR* addr,
                long port,
                CHAR* data,
                INT32 len);
  VOID EventNotify(OSAL_Socket_Handle sock,
                   mmBase::CbSocket::SOCKET_NOTIFYTYPE type);

  void CpuUsage(float cpu_usage);
  void Bandwidth(double speed) { bandwidth = speed; }
  void Mem(long int mem) { mem_ = mem; }
  void PeakMem(long int peak_mem) { peak_mem_ = peak_mem; }
  void VirtualMem(long int virtual_mem) { virtual_mem_ = virtual_mem; }
  void PeakVirtualMem(long int peak_virtual_mem) {
    peak_virtual_mem_ = peak_virtual_mem;
  }

  float CpuUsage();
  int CpuCores() { return cpu_cores_; }
  float Frequency() { return frequency_; }
  double Bandwidth() { return bandwidth; }
  long int Mem() { return mem_; }
  long int PeakMem() { return peak_mem_; }
  long int VirtualMem() { return virtual_mem_; }
  long int PeakVirtualMem() { return peak_virtual_mem_; }

 private:
  ServerSocket sock_;
  MonitorThread monitor_;

  // cpu
  std::list<float> cpu_usages_;
  int cpu_cores_;
  float frequency_;

  // network
  double bandwidth;

  // memory
  long int mem_;
  long int peak_mem_;
  long int virtual_mem_;
  long int peak_virtual_mem_;
};

#endif  // __INCLUDE_MONITOR_SERVER_H__
