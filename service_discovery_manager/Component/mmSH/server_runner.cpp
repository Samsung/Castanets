/*
 * Copyright 2018-2019 Samsung Electronics Co., Ltd
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

#include "server_runner.h"

#include "bINIParser.h"
#include "discovery_server.h"
#include "Dispatcher.h"
#include "monitor_server.h"
#include "osal.h"
#include "service_server.h"
#include "TPL_SGT.h"

#if defined (ENABLE_STUN)
#include "NetTunProc.h"
#endif

#if defined (WIN32)
#include "spawn_controller.h"
#endif

using namespace mmBase;
using namespace mmProto;

#define UUIDS_SDS "sds-0000"
#define UUIDS_MDS "sms-0000"
#define UUIDS_SRS "srs-0000"

static void OnDiscoveryServerEvent(int wParam,
                                   int lParam,
                                   void* pData,
                                   void* pParent) {
  DPRINT(CONN, DEBUG_INFO, "OnDiscoveryServerEvent : (%d)-(%d)-(%s)\n", wParam,
         lParam, (char*)pData);
}

ServerRunner::ServerRunner(ServerRunnerParams& params)
    : params_(params),
      keep_running_(true) {}

ServerRunner::~ServerRunner() {}

int ServerRunner::Initialize() {
  if (params_.is_daemon) {
    __OSAL_DaemonAPI_Daemonize("server-runner");
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CSTI<CbDispatcher>::getInstancePtr()->Initialize();

  return 0;
}

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
int ServerRunner::Run(HANDLE ev_term) {
#else
int ServerRunner::Run() {
#endif
  CDiscoveryServer* handle_discovery_server = new CDiscoveryServer(UUIDS_SDS);
  handle_discovery_server->SetServiceParam(params_.service_port,
                                           params_.monitor_port);
  if (!handle_discovery_server->StartServer(params_.multicast_addr.c_str(),
                                            params_.multicast_port)) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start discover server\n");
    return 1;
  }

  CbMessage* mh_discovery_server = GetThreadMsgInterface(UUIDS_SDS);
  CSTI<CbDispatcher>::getInstancePtr()->Subscribe(DISCOVERY_QUERY_EVENT,
                                                  (void*)mh_discovery_server,
                                                  OnDiscoveryServerEvent);

  MonitorServer* monitor_server = new MonitorServer(UUIDS_MDS);
  if (!monitor_server->Start(params_.monitor_port)) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start monitor server\n");
    return 1;
  }

  CServiceServer* handle_service_server =
      new CServiceServer(UUIDS_SRS, params_.exec_path.c_str());
  if (!handle_service_server->StartServer(params_.service_port)) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start service server\n");
    return 1;
  }

#if defined (ENABLE_STUN)
  CNetTunProc* pTunClient = NULL;
  if (params_.with_presence) {
    pTunClient = new CNetTunProc(
        "tunprocess",
        const_cast<char*>(params_.presence_addr.c_str()),
        params_.presence_port,
        10240, 10000, 1000, 3);
    pTunClient->SetRole(CRouteTable::RENDERER);
    pTunClient->Create();
  }
#endif

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
  while (WaitForSingleObject(ev_term, 0) != WAIT_OBJECT_0) {
#else
  while (true) {
#endif
    if (params_.is_daemon) {
      if (__OSAL_DaemonAPI_IsRunning() != 1) {
        break;
      }
    }

    if (!keep_running_)
      break;

    __OSAL_Sleep(1000);
  }

  handle_discovery_server->Close();
  SAFE_DELETE(handle_discovery_server);

  monitor_server->Stop();
  SAFE_DELETE(monitor_server);

  handle_service_server->StopServer();
  SAFE_DELETE(handle_service_server);

#if defined (ENABLE_STUN)
  SAFE_DELETE(pTunClient);
#endif

  return 0;
}

void ServerRunner::Stop() {
    keep_running_ = false;
}
