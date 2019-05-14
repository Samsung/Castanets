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

using namespace mmBase;

template <>
ServiceProvider* CSTI<ServiceProvider>::m_pInstance = NULL;

ServiceProvider::ServiceProvider() {}

ServiceProvider::~ServiceProvider() {}

VOID ServiceProvider::AddServiceInfo(CHAR* address,
                                     INT32 service_port,
                                     INT32 monitor_port) {
  ServiceInfo* info = new ServiceInfo;
  info->key = GenerateKey(address, service_port);
  if (CheckExisted(info->key))
    return;

  strncpy(info->address, address, strlen(address));
  info->service_port = service_port;
  info->monitor_port = monitor_port;
  service_providers_.AddTail(info);

  PrintServiceList();
}

ServiceInfo* ServiceProvider::GetServiceInfo(int index) {
  return service_providers_.GetAt(index);
}

ServiceInfo* ServiceProvider::ChooseBestService() {
  int count = service_providers_.GetCount();
  INT32 best_index = 0;
  double best_score = 0;

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
  return service_providers_.GetAt(best_index);
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
  INT32 pos = GetIndex(key);
  if (pos < 0)
    return FALSE;

  ServiceInfo* info = service_providers_.GetAt(pos);
  info->monitor.id = val->id;
  info->monitor.rtt = val->rtt;
  info->monitor.cpu_usage = val->cpu_usage;
  info->monitor.cpu_cores = val->cpu_cores;
  info->monitor.bandwidth = val->bandwidth;
  info->monitor.frequency = val->frequency;

  return TRUE;
}

INT32 ServiceProvider::Count() {
  return service_providers_.GetCount();
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

BOOL ServiceProvider::CheckExisted(UINT64 key) {
  int count = service_providers_.GetCount();

  for (int i = 0; i < count; i++) {
    ServiceInfo* info = service_providers_.GetAt(i);
    if (info->key == key)
      return TRUE;
  }
  return FALSE;
}

void ServiceProvider::PrintServiceList() {
  DPRINT(COMM, DEBUG_INFO, "=============== Service List ===============\n");
  DPRINT(COMM, DEBUG_INFO, "   address\tport(S)\tport(M)\n");
  DPRINT(COMM, DEBUG_INFO, "--------------------------------------------\n");

  int count = service_providers_.GetCount();
  for (int i = 0; i < count; i++) {
    auto info = service_providers_.GetAt(i);
    DPRINT(COMM, DEBUG_INFO, "%s\t%d\t%d\n",
           info->address, info->service_port, info->monitor_port);
  }

  DPRINT(COMM, DEBUG_INFO, "============================================\n");
}
