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

#ifndef WIN32
#if defined(ANDROID)
#include "ifaddrs.h"
#else
#include <ifaddrs.h>
#endif
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <sys/times.h>
#include <thread>
#endif

#include "monitor_server.h"

using namespace mmBase;
using namespace mmProto;

static unsigned long long last_total_user, last_total_user_low, last_total_sys,
    last_total_idle;

#if defined(ANDROID)
static inline __u32 ethtool_cmd_speed(const struct ethtool_cmd *ep) {
  return (ep->speed_hi << 16) | ep->speed;
}
#endif

ServerSocket::ServerSocket(MonitorServer* parent)
    : CpTcpServer(), parent_(parent) {}

ServerSocket::ServerSocket(MonitorServer* parent, const CHAR* msg_name)
    : CpTcpServer(msg_name), parent_(parent) {}

bool ServerSocket::MakeMonitiorInfo() {
  if (!parent_)
    return false;

  monitor_info_.clear();
  monitor_info_ = "USAGE=";
  monitor_info_ += std::to_string(parent_->CpuUsage());
  monitor_info_ += ";";

  monitor_info_ += "CORES=";
  monitor_info_ += std::to_string(parent_->CpuCores());
  monitor_info_ += ";";

  monitor_info_ += "BANDWIDTH=";
  monitor_info_ += std::to_string(parent_->Bandwidth());
  monitor_info_ += ";";

  monitor_info_ += "FREQ=";
  monitor_info_ += std::to_string(parent_->Frequency());
  monitor_info_ += ";";

  return true;
}
VOID ServerSocket::DataRecv(OSAL_Socket_Handle sock,
                            const CHAR* addr,
                            long port,
                            CHAR* data,
                            INT32 len) {
  DPRINT(COMM, DEBUG_INFO, "Receive- from:[%d-%s] msg:[%s]\n", sock,
         Address(sock), data);

  if (!strncmp(data, "QUERY-MONITORING", strlen("QUERY-MONITORING")) &&
      MakeMonitiorInfo()) {
    char buf_[MAX_MONITOR_MSG_BUFF] = {
        '\0',
    };
    strncpy(buf_, monitor_info_.c_str(), monitor_info_.length());
    CpTcpServer::DataSend(sock, buf_, monitor_info_.length());
  }
}

VOID ServerSocket::EventNotify(OSAL_Socket_Handle sock,
                               CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify- form:sock[%d] event[%d]\n", sock, type);
}

MonitorThread::MonitorThread(MonitorServer* parent)
    : CbThread(), parent_(parent) {}

MonitorThread::MonitorThread(MonitorServer* parent, const CHAR* name)
    : CbThread(name), parent_(parent) {}

void MonitorThread::MainLoop(void* args) {
  while (m_bRun) {
    CheckBandwidth();
    CheckCpuUsage();
    CheckMemoryUsage();
    __OSAL_Sleep(SERVER_MONITORING_TIME);
  }
}

void MonitorThread::CheckBandwidth() {
#if !defined(WIN32)
#if defined(ANDROID)
  struct mmBase::ifaddrs *ifap, *ifa;
#else
  struct ifaddrs *ifap, *ifa;
#endif
  struct ifreq ifr;
  struct ethtool_cmd edata;
  int sock, rc;
  double max_speed = 0;

#if defined(ANDROID)
  if (mmBase::getifaddrs(&ifap) == -1) {
#else
  if (getifaddrs(&ifap) == -1) {
#endif
    DPRINT(COMM, DEBUG_ERROR, "Failed to getifaddrs() - errno(%d)\n", errno);
    return;
  }
  for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
    double current_max_speed = 0;
    if (ifa->ifa_addr->sa_family == AF_INET) {
      sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
      if (sock < 0) {
        DPRINT(COMM, DEBUG_ERROR, "sock error\n");
        return;
      }

      if (!strncmp(ifa->ifa_name, "eth", 3)) {
        strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
        ifr.ifr_data = &edata;

        edata.cmd = ETHTOOL_GSET;

        rc = ioctl(sock, SIOCETHTOOL, &ifr);
        if (rc < 0) {
          DPRINT(COMM, DEBUG_ERROR, "ioctl error\n");
          close(sock);
          return;
        }
        ethtool_cmd_speed(&edata);
        current_max_speed = edata.speed * 100;  // convert to kbps
      } else if (!strncmp(ifa->ifa_name, "wlan", 4)) {
        // TODO (djmix.kim) : For now, set 30Mbps by force in wifi.
        current_max_speed = 30000;
      } else {
        // TODO (djmix.kim) : How to check mobile network (3g, 4g...)?
      }

      if (max_speed < current_max_speed)
        max_speed = current_max_speed;

      close(sock);
    }
  }
#if defined(ANDROID)
  mmBase::freeifaddrs(ifap);
#else
  freeifaddrs(ifap);
#endif

  if (parent_)
    parent_->Bandwidth(max_speed);
#else
  if (parent_)
    parent_->Bandwidth(0);
#endif  // !defined(WIN32)
}

void MonitorThread::CheckMemoryUsage() {
  long int mem, peak_mem, virtual_mem, peak_virtual_mem;
#if !defined(WIN32)
  char buffer[1024] = "";
  FILE* file;

  if ((file = fopen("/proc/self/status", "r")) != NULL) {
    while (fscanf(file, " %1023s", buffer) == 1) {
      if (!strncmp(buffer, "VmRSS:", strlen("VmRSS:")))
        fscanf(file, " %ld", &mem);
      else if (!strncmp(buffer, "VmHWM:", strlen("VmHWM:")))
        fscanf(file, " %ld", &peak_mem);
      else if (!strncmp(buffer, "VmSize:", strlen("VmSize:")))
        fscanf(file, " %ld", &virtual_mem);
      else if (!strncmp(buffer, "VmPeak:", strlen("VmPeak:")))
        fscanf(file, " %ld", &peak_virtual_mem);
    }

    fclose(file);
    DPRINT(COMM, DEBUG_INFO,
         "Memory Usage : VmRSS:[%ld] VmHWM:[%ld] VmSize:[%ld] VmPeak:[%ld]\n",
          mem, peak_mem, virtual_mem, peak_virtual_mem);
  } else {
      DPRINT(COMM, DEBUG_ERROR,
         "Could not open /proc/self/status - errno(%d)\n", errno);
    mem = peak_mem = virtual_mem = peak_virtual_mem = 0;
  }
#else
  // TODO
  mem = peak_mem = virtual_mem = peak_virtual_mem = 0;
#endif // !defined(WIN32)
  if (parent_) {
    parent_->Mem(mem);
    parent_->PeakMem(peak_mem);
    parent_->VirtualMem(virtual_mem);
    parent_->PeakVirtualMem(peak_virtual_mem);
  }
}

void MonitorThread::CheckCpuUsage() {
  double cpu_usage;
// TODO(djmix.kim) : Android API 23 does not support accessing "/proc/stat".
// We disable current function for a while.
#if !defined(WIN32) && !defined(ANDROID)
  FILE* file;
  unsigned long long total_user, total_user_low, total_sys, total_idle, total;
  DPRINT(COMM, DEBUG_ERROR, "test\n");

  if ((file = fopen("/proc/stat", "r")) == NULL) {
    DPRINT(COMM, DEBUG_ERROR, "Could not open /proc/stat - errno(%d)\n", errno);
    return;
  }
  fscanf(file, "cpu %llu %llu %llu %llu", &total_user, &total_user_low,
         &total_sys, &total_idle);
  fclose(file);

  if (total_user < last_total_user || total_user_low < last_total_user_low ||
      total_sys < last_total_sys || total_idle < last_total_idle) {
    // Overflow detection. Just skip this value.
    cpu_usage = -1.0;
  } else {
    total = (total_user - last_total_user) +
            (total_user_low - last_total_user_low) +
            (total_sys - last_total_sys);
    cpu_usage = total;
    total += (total_idle - last_total_idle);
    cpu_usage /= total;
  }

  last_total_user = total_user;
  last_total_user_low = total_user_low;
  last_total_sys = total_sys;
  last_total_idle = total_idle;
#else
  cpu_usage = 0.1;
#endif // !defined(WIN32) && !defined(ANDROID)
  if (parent_ && cpu_usage >= 0) {
    DPRINT(COMM, DEBUG_INFO, "CPU Usage : [%.2lf] \n", cpu_usage * 100);
    parent_->CpuUsage((float)cpu_usage);
  }
}

MonitorServer::MonitorServer()
    : sock_(this),
      monitor_(this, "MonitorThreadServer"),
      bandwidth(0),
      mem_(0),
      peak_mem_(0),
      virtual_mem_(0),
      peak_virtual_mem_(0) {
  monitor_.StartMainLoop(nullptr);
  cpu_usages_.clear();

#if defined(ANDROID)
  // TODO(djmix.kim) : Android API 23 does not support accessing "/proc/stat".
  cpu_cores_ = sysconf(_SC_NPROCESSORS_ONLN);

  // get cpu frequency
  double frequency = 0;
  FILE* file;
  if ((file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r")) != NULL) {
    fscanf(file, "%lf", &frequency);
    fclose(file);
    frequency_ = (float)(frequency / 1000000);
  } else {
    DPRINT(COMM, DEBUG_ERROR,
       "Could not open /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq - errno(%d)\n", errno);
    frequency_ = 1.0f;
  }
#elif defined(LINUX)
  // initialize for cpu usage
  FILE* file;
  if ((file = fopen("/proc/stat", "r")) != NULL) {
    fscanf(file, "cpu %llu %llu %llu %llu", &last_total_user,
           &last_total_user_low, &last_total_sys, &last_total_idle);
    fclose(file);

    // get cpu core
    // TODO (djmix.kim) : How to get active cores?
    cpu_cores_ = std::thread::hardware_concurrency();
  } else {
    DPRINT(COMM, DEBUG_ERROR,
       "Could not open /proc/stat - errno(%d)\n", errno);
    cpu_cores_ = 1;
  }

  // get cpu frequency
  double frequency = 0;
  if ((file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r")) != NULL) {
    fscanf(file, "%lf", &frequency);
    fclose(file);
    frequency_ = (float)(frequency / 1000000);
  } else {
    DPRINT(COMM, DEBUG_ERROR,
       "Could not open /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq - errno(%d)\n", errno);
    frequency_ = 1.0f;
  }
#else
  cpu_cores_ = 1;
  frequency_ = 1.0f;
#endif
}

MonitorServer::MonitorServer(const CHAR* msg_name)
    : sock_(this, msg_name),
      monitor_(this, "MonitorThreadServer"),
      bandwidth(0),
      mem_(0),
      peak_mem_(0),
      virtual_mem_(0),
      peak_virtual_mem_(0) {
  monitor_.StartMainLoop(nullptr);
  cpu_usages_.clear();

#if defined(ANDROID)
  // TODO(djmix.kim) : Android API 23 does not support accessing "/proc/stat".
  cpu_cores_ = sysconf(_SC_NPROCESSORS_ONLN);

  // get cpu frequency
  double frequency = 0;
  FILE* file;
  if ((file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r")) != NULL) {
    fscanf(file, "%lf", &frequency);
    fclose(file);
    frequency_ = (float)(frequency / 1000000);
  } else {
    DPRINT(COMM, DEBUG_ERROR,
       "Could not open /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq - errno(%d)\n", errno);
    frequency_ = 1.0f;
  }
#elif defined(LINUX)
  // initialize for cpu usage
  FILE* file;
  if ((file = fopen("/proc/stat", "r")) != NULL) {
    fscanf(file, "cpu %llu %llu %llu %llu", &last_total_user,
           &last_total_user_low, &last_total_sys, &last_total_idle);
    fclose(file);

    // get cpu core
    // TODO (djmix.kim) : How to get active cores?
    cpu_cores_ = std::thread::hardware_concurrency();
  } else {
    DPRINT(COMM, DEBUG_ERROR,
       "Could not open /proc/stat - errno(%d)\n", errno);
    cpu_cores_ = 1;
  }

  // get cpu frequency
  double frequency = 0;
  if ((file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r")) != NULL) {
    fscanf(file, "%lf", &frequency);
    fclose(file);
    frequency_ = (float)(frequency / 1000000);
  } else {
    DPRINT(COMM, DEBUG_ERROR,
       "Could not open /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq - errno(%d)\n", errno);
    frequency_ = 1.0f;
  }
#else
  cpu_cores_ = 1;
  frequency_ = 1.0f;
#endif
}

MonitorServer::~MonitorServer() {
  monitor_.StopMainLoop();
}

BOOL MonitorServer::Start(int port, int read) {
  DPRINT(COMM, DEBUG_INFO, "start monitor server with [%d] port\n", port);
  sock_.Create();
  sock_.Open(port);
  sock_.Start(read);
  return TRUE;
}

BOOL MonitorServer::Stop() {
  return TRUE;
}

void MonitorServer::CpuUsage(float cpu_usage) {
  if (cpu_usages_.size() > 5)
    cpu_usages_.pop_front();

  cpu_usages_.push_back(cpu_usage);
}

float MonitorServer::CpuUsage() {
  float sum = 0;
  for (auto it = cpu_usages_.begin(); it != cpu_usages_.end(); ++it)
    sum += *it;

  return sum / cpu_usages_.size();
}
