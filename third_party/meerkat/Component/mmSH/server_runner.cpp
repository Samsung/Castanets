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

#include "TPL_SGT.h"
#include "bINIParser.h"
#include "discovery_server.h"
#include "monitor_server.h"
#include "osal.h"
#include "service_server.h"

#if defined(ENABLE_STUN)
#include "NetTunProc.h"
#endif

#if defined(WIN32)
#include "spawn_controller.h"
#endif

using namespace mmBase;
using namespace mmProto;

#define UUIDS_SDS "sds-0000"
#define UUIDS_MDS "sms-0000"
#define UUIDS_SRS "srs-0000"

// static
bool ServerRunner::BuildParams(const std::string& ini_path,
                               ServerRunnerParams& params) {
  mmBase::CbINIParser settings;

  int ret = settings.Parse(ini_path);
  if (ret !=0) {
    DPRINT(COMM, DEBUG_ERROR, "ini parse error(%d)\n", ret);
    return false;
  }

  params.multicast_addr = settings.GetAsString("multicast", "address", "");
  params.multicast_port = settings.GetAsInteger("multicast", "port", -1);
  params.service_port = settings.GetAsInteger("service", "port", -1);
  params.exec_path = settings.GetAsString("service", "exec-path", "");
  params.monitor_port = settings.GetAsInteger("monitor", "port", -1);
  params.presence_addr = settings.GetAsString("presence", "address", "");
  params.presence_port = settings.GetAsInteger("presence", "port", -1);
  params.with_presence = params.presence_addr.length() > 0 &&
                         params.presence_port > 0;
  params.is_daemon = settings.GetAsBoolean("run", "run-as-damon", false);

  return true;
}

// static
bool ServerRunner::BuildParams(int argc, char** argv,
                               ServerRunnerParams& params) {
  if (argc < 5) {
    DPRINT(COMM, DEBUG_ERROR, "Too Few Argument!!\n");
    DPRINT(COMM, DEBUG_ERROR, "usage : %s mc_addr mc_port svc_port mon_port"
           "<presence> <pr_addr> <pr_port> <daemon>\n", argv[0]);
    DPRINT(COMM, DEBUG_ERROR,
           "comment: mc(multicast), svc(service), mon(monitor)\n");
    DPRINT(COMM, DEBUG_ERROR, "         presence (default is 0."
           "You need to come with pr_addr and pr_port when you use it)\n");
    DPRINT(COMM, DEBUG_ERROR, "         daemon (default is 0."
           " You can use it if you want\n");
    return false;
  }

  params.multicast_addr = std::string(argv[1]);
  params.multicast_port = atoi(argv[2]);
  params.service_port = atoi(argv[3]);
  params.monitor_port = atoi(argv[4]);
  params.is_daemon = (argc == 6 && (strncmp(argv[5], "daemon", 6) == 0)) ||
                     (argc == 9 && (strncmp(argv[8], "daemon", 6) == 0));
  params.with_presence = (argc >= 8 && (strncmp(argv[5], "presence", 8) == 0));
  if (params.with_presence) {
    params.presence_addr = std::string(argv[6]);
    params.presence_port = atoi(argv[7]);
  }

  return true;
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

  return 0;
}

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
int ServerRunner::Run(HANDLE ev_term) {
#else
int ServerRunner::Run() {
#endif
  if (!BeforeRun())
    return 1;

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

  AfterRun();

  return 0;
}

void ServerRunner::Stop() {
    keep_running_ = false;
}

bool ServerRunner::BeforeRun() {
  discovery_server_.reset(new CDiscoveryServer(UUIDS_SDS));
  discovery_server_->
      SetServiceParam(params_.service_port,
                      params_.monitor_port,
                      params_.get_capability);
  if (!discovery_server_->
      StartServer(params_.multicast_addr.c_str(), params_.multicast_port)) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start discovery server!\n");
    return false;
  }

  service_server_.reset(
      new CServiceServer(UUIDS_SRS,
                         params_.exec_path.c_str(),
                         params_.get_token,
                         params_.verify_token));
  if (!service_server_->StartServer(params_.service_port)) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start service server!\n");
    discovery_server_->StopServer();
    return false;
  }

#if defined (ENABLE_STUN)
  if (params_.with_presence) {
    tun_client_.reset(
        new CNetTunProc("tunprocess",
                        const_cast<char*>(params_.presence_addr.c_str()),
                        params_.presence_port,
                       10240, 10000, 1000, 3);
    tun_client_->SetRole(CRouteTable::RENDERER);
    tun_client_->Create();
  }
#endif

  return true;
}

void ServerRunner::AfterRun() {
  discovery_server_->StopServer();
  service_server_->StopServer();
}
