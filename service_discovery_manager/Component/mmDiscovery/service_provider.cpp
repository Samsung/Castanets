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

#include "service_provider.h"

#include <math.h>
#include <iostream>
#include <sstream>

#include "timeAPI.h"

using namespace mmBase;

static const UINT64 kExpiresMs = 3 * 1000;

ServiceInfo::ServiceInfo()
    : key(0),
      address({0,}),
      service_port(-1),
      monitor_port(-1),
      last_update_time(0) {}

ServiceInfo::~ServiceInfo() {
}

ServiceProvider::ServiceProvider()
    : mutex_(__OSAL_Mutex_Create()) {
}

ServiceProvider::~ServiceProvider() {
  __OSAL_Mutex_Destroy(&mutex_);
}

VOID ServiceProvider::AddServiceInfo(CHAR* address,
                                     INT32 service_port,
                                     INT32 monitor_port) {
  UINT64 key = GenerateKey(address, service_port);
  INT32 index;

  __OSAL_Mutex_Lock(&mutex_);
  if ((index = GetIndex(key)) >= 0) {
    ServiceInfo* info = service_providers_.GetAt(index);
    __OSAL_TIME_GetTimeMS(&info->last_update_time);
    __OSAL_Mutex_UnLock(&mutex_);
    return;
  }

  ServiceInfo* new_info = new ServiceInfo;

  new_info->key = key;
  strncpy(new_info->address, address, strlen(address));
  new_info->service_port = service_port;
  new_info->monitor_port = monitor_port;
  __OSAL_TIME_GetTimeMS(&new_info->last_update_time);
  service_providers_.AddTail(new_info);

  PrintServiceList();
  __OSAL_Mutex_UnLock(&mutex_);
}

ServiceInfo* ServiceProvider::GetServiceInfo(int index) {
  ServiceInfo* info;
  __OSAL_Mutex_Lock(&mutex_);
  info = service_providers_.GetAt(index);
  __OSAL_Mutex_UnLock(&mutex_);
  return info;
}

ServiceInfo* ServiceProvider::ChooseBestService() {
  INT32 best_index = -1;
  double best_score = 0;

  __OSAL_Mutex_Lock(&mutex_);
  int count = service_providers_.GetCount();
  for (int i = 0; i < count; i++) {
    ServiceInfo* info = service_providers_.GetAt(i);

    // Score calculation for service decision
    //  - page loading score = (network score + cpu score) / 2
    //  - score = page loading score + rendering score
    double score = (NetworkScore(info->monitor.bandwidth) +
                    CpuScore(info->monitor.frequency,
                             info->monitor.cpu_usage,
                             info->monitor.cpu_cores)) / 2 +
                    RenderingScore(info->monitor.rtt);

    if (i == 0) {
      best_index = i;
      best_score = score;
    } else if (best_score > score) {
      best_index = i;
      best_score = score;
    }
  }

  DPRINT(COMM, DEBUG_INFO, "ChooseBestService - index(%d) score(%d)\n",
      best_index, best_score);
  ServiceInfo* info = service_providers_.GetAt(best_index);
  __OSAL_Mutex_UnLock(&mutex_);
  return info;
}

double ServiceProvider::NetworkScore(double n) {
  return 1 / (8770 * pow(n, -0.9));
}

double ServiceProvider::CpuScore(float f, float u, int c) {
  return ((1 / (5.66 * pow(f, -0.66))) +
          (1 / (3.22 * pow(u, -0.241))) +
          (1 / (4 * pow(c, -0.3)))) / 3;
}

double ServiceProvider::RenderingScore(double r) {
  return (r < 0) ? 0 : 0.77 * pow(r, -0.43);
}

BOOL ServiceProvider::UpdateServiceInfo(UINT64 key, MonitorInfo* val) {
  __OSAL_Mutex_Lock(&mutex_);
  INT32 pos = GetIndex(key);
  if (pos < 0) {
    __OSAL_Mutex_UnLock(&mutex_);
    return FALSE;
  }

  ServiceInfo* info = service_providers_.GetAt(pos);
  info->monitor.id = val->id;
  info->monitor.rtt = val->rtt;
  info->monitor.cpu_usage = val->cpu_usage;
  info->monitor.cpu_cores = val->cpu_cores;
  info->monitor.bandwidth = val->bandwidth;
  info->monitor.frequency = val->frequency;
  __OSAL_TIME_GetTimeMS(&info->last_update_time);
  __OSAL_Mutex_UnLock(&mutex_);
  return TRUE;
}

INT32 ServiceProvider::Count() {
  INT32 count;
  __OSAL_Mutex_Lock(&mutex_);
  count = service_providers_.GetCount();
  __OSAL_Mutex_UnLock(&mutex_);
  return count;
}

UINT64 ServiceProvider::GenerateKey(CHAR* str, INT32 index) {
  std::stringstream s(str);
  UINT32 a, b, c, d;  // to store the 4 ints
  CHAR ch;            // to temporarily store the '.'
  s >> a >> ch >> b >> ch >> c >> ch >> d;
  UINT64 h = a << 24 | b << 16 | c << 8 | d;
  UINT64 key = h << 32 | index;
  return key;
}

INT32 ServiceProvider::GetIndex(UINT64 key) {
  int count = service_providers_.GetCount();

  for (int i = 0; i < count; i++) {
    ServiceInfo* info = service_providers_.GetAt(i);
    if (info->key == key)
      return i;
  }
  return -1;
}

void ServiceProvider::InvalidateServiceList() {
  UINT64 current_time = 0;
  __OSAL_TIME_GetTimeMS(&current_time);

  __OSAL_Mutex_Lock(&mutex_);
  int count = service_providers_.GetCount();
  for (int i = 0; i < count;) {
    auto* info = service_providers_.GetAt(i);
    if (current_time - info->last_update_time >= kExpiresMs) {
      DPRINT(COMM, DEBUG_INFO, "Service(%s) has been removed"
             " due to time expired.\n", info->address);
      if (service_providers_.DelAt(i) == 0)
        break;
    } else {
      ++i;
    }
  }

  if (count != service_providers_.GetCount())
    PrintServiceList();
  __OSAL_Mutex_UnLock(&mutex_);
}

void ServiceProvider::PrintServiceList() {
  DPRINT(COMM, DEBUG_INFO, "============= Service List =============\n");
  DPRINT(COMM, DEBUG_INFO, "   address\tport(S)\tport(M)\n");
  DPRINT(COMM, DEBUG_INFO, "----------------------------------------\n");

  int count = service_providers_.GetCount();
  for (int i = 0; i < count; i++) {
    auto* info = service_providers_.GetAt(i);
    DPRINT(COMM, DEBUG_INFO, "%s\t%d\t%d\n",
           info->address, info->service_port, info->monitor_port);
  }

  DPRINT(COMM, DEBUG_INFO, "========================================\n");
}
