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

#include "bINIParser.h"
#include "discovery_server.h"
#include "Dispatcher.h"
#include "monitor_server.h"
#include "NetTunProc.h"
#include "osal.h"
#include "service_server.h"
#include "TPL_SGT.h"

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

int main(int argc, char** argv) {
  CbINIParser settings;
  int ret;
  const char* multicast_addr = NULL;
  const char* service_path = NULL;
  const char* presence_addr = NULL;
  std::string multicast_addr_string;
  std::string service_path_string;
  std::string presence_addr_string;
  int multicast_port, service_port, monitor_port, presence_port;
  bool is_daemon;
  bool with_presence = true;
  ret = settings.Parse("server.ini");
  if (ret == -1)
    ret = settings.Parse("/usr/bin/server.ini");

  if (!ret) {
    multicast_addr_string = settings.GetAsString("multicast", "address", "");
    multicast_addr = multicast_addr_string.c_str();
    if (strlen(multicast_addr) == 0) {
      RAW_PRINT("No multicast-address in settings.ini\n");
      return 0;
    }
    multicast_port = settings.GetAsInteger("multicast", "port", -1);
    if (multicast_port == -1) {
      RAW_PRINT("No multicast-port in settings.ini\n");
      return 0;
    }
    presence_addr_string = settings.GetAsString("presence", "address", "");
    presence_addr = presence_addr_string.c_str();
    if (strlen(presence_addr) == 0) {
      RAW_PRINT("No presence-address in settings.ini\n");
      with_presence = false;
    }
    presence_port = settings.GetAsInteger("presence", "port", -1);
    if (presence_port == -1) {
      RAW_PRINT("No presence-port in settings.ini\n");
      with_presence = false;
    }
    is_daemon = settings.GetAsBoolean("run", "run-as-damon", false);
    service_port = settings.GetAsInteger("service", "port", -1);
    if (service_port == -1) {
      RAW_PRINT("No service-port in settings.ini\n");
      return 0;
    }
    monitor_port = settings.GetAsInteger("monitor", "port", -1);
    if (monitor_port == -1) {
      RAW_PRINT("No monitor-port in settings.ini\n");
      return 0;
    }
    service_path_string = settings.GetAsString("service", "exec-path", "");
    service_path = service_path_string.c_str();
    if (strlen(service_path) == 0) {
      RAW_PRINT("No service-path in settings.ini\n");
      return 0;
    }
  } else {
    RAW_PRINT("ini parse error(%d)\n", ret);
    if (argc < 5) {
      RAW_PRINT("Too Few Argument!!\n");
      RAW_PRINT("usage : %s mc_addr mc_port svc_port mon_port <presence> "
                "<pr_addr> <pr_port> <daemon>\n", argv[0]);
      RAW_PRINT("comment: mc(multicast), svc(service), mon(monitor)\n");
      RAW_PRINT("         presence (default is 0. You need to come with "
                "pr_addr and pr_port when you use it)\n");
      RAW_PRINT("         daemon (default is 0. You can use it if you want\n");
      return 0;
    }
    multicast_addr = argv[1];
    multicast_port = atoi(argv[2]);
    service_port = atoi(argv[3]);
    monitor_port = atoi(argv[4]);
    is_daemon = (argc == 6 && (strncmp(argv[5], "daemon", 6) == 0)) ||
                (argc == 9 && (strncmp(argv[8], "daemon", 6) == 0));
    with_presence =  (argc >= 8 && (strncmp(argv[5], "presence", 8) == 0));
    if (with_presence) {
      presence_addr = argv[6];
      presence_port = atoi(argv[7]);
    }
  }

  if (is_daemon) {
    __OSAL_DaemonAPI_Daemonize("server-runner");
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CSTI<CbDispatcher>::getInstancePtr()->Initialize();

  CDiscoveryServer* handle_discovery_server = new CDiscoveryServer(UUIDS_SDS);
  handle_discovery_server->SetServiceParam(service_port, monitor_port);
  if (!handle_discovery_server->StartServer(multicast_addr, multicast_port)) {
    RAW_PRINT("cannot start discover server\n");
    return 0;
  }

  CbMessage* mh_discovery_server = GetThreadMsgInterface(UUIDS_SDS);
  CSTI<CbDispatcher>::getInstancePtr()->Subscribe(DISCOVERY_QUERY_EVENT,
                                                  (void*)mh_discovery_server,
                                                  OnDiscoveryServerEvent);

  MonitorServer* monitor_server = new MonitorServer(UUIDS_MDS);
  if (!monitor_server->Start(monitor_port)) {
    RAW_PRINT("cannot start monitor server\n");
    return 0;
  }

  CServiceServer* handle_service_server =
      new CServiceServer(UUIDS_SRS, service_path);
  if (!handle_service_server->StartServer(service_port)) {
    RAW_PRINT("Cannot start service server\n");
    return 0;
  }

  CNetTunProc* pTunClient = NULL;
  if (with_presence) {
    pTunClient = 
        new CNetTunProc("tunprocess", (char*)presence_addr, presence_port,
                        10240, 10000, 1000, 3);
    pTunClient->SetRole(CRouteTable::RENDERER);
    pTunClient->Create();
  }

  while (true) {
    if (is_daemon) {
      if (__OSAL_DaemonAPI_IsRunning() != 1) {
        break;
      }
    } else {
      RAW_PRINT("If you want to quit press 'q'\n");
      CHAR user_input = getchar();
      if (user_input == 'q') {
        RAW_PRINT("Quit Program\n");
        break;
      }
    }
  }

  handle_discovery_server->Close();
  SAFE_DELETE(handle_discovery_server);

  monitor_server->Stop();
  SAFE_DELETE(monitor_server);

  handle_service_server->StopServer();
  SAFE_DELETE(handle_service_server);

  SAFE_DELETE(pTunClient);
  return 0;
}
