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

#ifndef __INCLUDE_SERVICE_PROVIDER_H__
#define __INCLUDE_SERVICE_PROVIDER_H__

#include "bDataType.h"
#include "monitor_client.h"
#include "service_client.h"
#include "TPL_SGT.h"

struct ServiceInfo {
  ServiceInfo();
  ~ServiceInfo();
  UINT64 key;
  CServiceClient* service_client;
  MonitorInfo monitor;
  UINT64 last_update_time;
  bool authorized;
};

class ServiceProvider : public CSTI<ServiceProvider> {
public:
  ServiceProvider();
  virtual ~ServiceProvider();

  VOID AddServiceInfo(CHAR* address,
                      INT32 service_port,
                      GetTokenFunc get_token,
                      VerifyTokenFunc verify_token);
  ServiceInfo* GetServiceInfo(INT32 index);
  ServiceInfo* ChooseBestService();
  double NetworkScore(double bandwidth);
  double CpuScore(float frequency, float usages, int cores);
  double RenderingScore(double rtt);
  BOOL UpdateServiceInfo(UINT64 key, MonitorInfo* val);
  void RemoveServiceInfo(unsigned long long key);
  INT32 Count();
  UINT64 GenerateKey(CHAR* str, INT32 index);
  void InvalidateServiceList();

 private:
  INT32 GetIndex(UINT64 key);
  void PrintServiceList();

  OSAL_Mutex_Handle mutex_;
  mmBase::CbList<ServiceInfo> service_providers_;
};
#endif  // __INCLUDE_SERVICE_PROVIDER_H__
