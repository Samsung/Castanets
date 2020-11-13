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
#include <string>

#include "timeAPI.h"

using namespace mmBase;
using namespace std;

static const UINT64 kExpiresMs = 3 * 1000;

static const char* StateToString(CServiceClient::State state) {
  switch (state) {
    case CServiceClient::NONE:
      return "None";
    case CServiceClient::CONNECTING:
      return "Connecting";
    case CServiceClient::CONNECTED:
      return "Connected";
    case CServiceClient::DISCONNECTED:
      return "Disconnected";
    default:
      return "";
  }
}

ServiceInfo::ServiceInfo()
    : key(0),
      service_client(nullptr),
      last_update_time(0),
      authorized(false) {}

ServiceInfo::~ServiceInfo() {
  if (service_client)
    delete service_client;
}

ServiceProvider::ServiceProvider()
    : get_token_(nullptr),
      verify_token_(nullptr),
      mutex_(__OSAL_Mutex_Create()) {
}

ServiceProvider::~ServiceProvider() {
  __OSAL_Mutex_Destroy(&mutex_);
}

void ServiceProvider::SetCallbacks(GetTokenFunc get_token,
                                   VerifyTokenFunc verify_token) {
  get_token_ = get_token;
  verify_token_ = verify_token;
}

void ServiceProvider::AddServiceInfo(const string& address,
                                     int service_port,
                                     const string& capability) {
  UINT64 key = GenerateKey(address, service_port);
  INT32 index;

  __OSAL_Mutex_Lock(&mutex_);
  if ((index = GetIndex(key)) >= 0) {
    ServiceInfo* info = service_providers_.GetAt(index);
      if (info->capability != capability)
        info->capability = capability;
    __OSAL_TIME_GetTimeMS(&info->last_update_time);
    __OSAL_Mutex_UnLock(&mutex_);
    return;
  }
  __OSAL_Mutex_UnLock(&mutex_);

  ServiceInfo* new_info = new ServiceInfo;

  new_info->key = key;
  new_info->service_client = new CServiceClient(std::to_string(key).c_str(),
                                                get_token_, verify_token_);
  if (!new_info->service_client->StartClient(address.c_str(), service_port)) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start service client for (%s:%d)!\n",
           address.c_str(), service_port);
    delete new_info;
    return;
  }
  new_info->capability = capability;
  __OSAL_TIME_GetTimeMS(&new_info->last_update_time);
  __OSAL_Mutex_Lock(&mutex_);
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

ServiceInfo* ServiceProvider::GetServiceInfo(const char* address) {
  ServiceInfo* info = nullptr;
  __OSAL_Mutex_Lock(&mutex_);
  int count = service_providers_.GetCount();
  for (int i = 0; i < count; i++) {
    info = service_providers_.GetAt(i);
    if (info->service_client->GetState() != CServiceClient::CONNECTED)
      continue;
    if (!strncmp(info->service_client->GetServerAddress(), address,
                 strlen(address))) {
      break;
    }
  }
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

    if (info->service_client->GetState() != CServiceClient::CONNECTED)
      continue;

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

  DPRINT(COMM, DEBUG_INFO,
         "ChooseBestService - index(%d) score(%lf)\n", best_index, best_score);
  ServiceInfo* info = (best_score >= 0) ?
      service_providers_.GetAt(best_index) : nullptr;
  __OSAL_Mutex_UnLock(&mutex_);
  return info;
}

double ServiceProvider::NetworkScore(double n) {
  return (n <= 0) ? 0 : 1 / (8770 * pow(n, -0.9));
}

double ServiceProvider::CpuScore(float f, float u, int c) {
  return (f <= 0 || u <= 0 || c <= 0) ? 0 : ((1 / (5.66 * pow(f, -0.66))) +
                                            (1 / (3.22 * pow(u, -0.241))) +
                                            (1 / (4 * pow(c, -0.3)))) / 3;
}

double ServiceProvider::RenderingScore(double r) {
  return (r <= 0) ? 0 : 0.77 * pow(r, -0.43);
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

void ServiceProvider::RemoveServiceInfo(unsigned long long key) {
  __OSAL_Mutex_Lock(&mutex_);
    INT32 pos = GetIndex(key);
  if (pos >= 0)
    service_providers_.DelAt(pos);
}
INT32 ServiceProvider::Count() {
  INT32 count;
  __OSAL_Mutex_Lock(&mutex_);
  count = service_providers_.GetCount();
  __OSAL_Mutex_UnLock(&mutex_);
  return count;
}

UINT64 ServiceProvider::GenerateKey(const string& str, int index) {
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
    bool should_remove = false;
    if (info->service_client->GetState() == CServiceClient::DISCONNECTED)
      should_remove = true;
    else if (current_time - info->last_update_time >= kExpiresMs &&
             info->service_client->GetState() == CServiceClient::NONE)
      should_remove = true;

    if (should_remove) {
      DPRINT(COMM, DEBUG_INFO,
             "Service(%s:%d) has been removed.\n",
             info->service_client->GetServerAddress(),
             info->service_client->GetServerPort());
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
  DPRINT(COMM, DEBUG_INFO, "   address\tport\tstate\n");
  DPRINT(COMM, DEBUG_INFO, "----------------------------------------\n");

  int count = service_providers_.GetCount();
  for (int i = 0; i < count; i++) {
    auto* info = service_providers_.GetAt(i);
    DPRINT(COMM, DEBUG_INFO, "%s\t%d\t%s\n",
           info->service_client->GetServerAddress(),
           info->service_client->GetServerPort(),
           StateToString(info->service_client->GetState()));
  }

  DPRINT(COMM, DEBUG_INFO, "========================================\n");
}
