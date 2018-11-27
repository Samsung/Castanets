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

#include <dbus/dbus.h>

#include "bINIParser.h"
#include "discovery_client.h"
#include "Dispatcher.h"
#include "monitor_client.h"
#include "NetTunProc.h"
#include "netUtil.h"
#include "osal.h"
#include "service_client.h"
#include "service_provider.h"
#include "TPL_SGT.h"

using namespace mmBase;
using namespace mmProto;

#define UUIDS_SDC "sdc-0000"
#define UUIDS_MDC "mdc-00%d"
#define UUIDS_SRC "src-0000"

typedef struct Monitor_ {
  MonitorClient* client;
  CHAR id[16];
  CHAR address[16];
  INT32 service_port;
  INT32 monitor_port;
} Monitor;

CbList<Monitor> monitor_manager;

static void OnMonitorClientEvent(int wParam,
                             int lParam,
                             void* pData,
                             void* pParent) {
  MonitorInfo* info = (MonitorInfo*)pData;
  DPRINT(CONN, DEBUG_INFO, "OnMonitorClientEvent : (%s)-(%.4lf)-(%.2f)-(%d)-"
      "(%.2f)-(%.2lf)\n", info->id.c_str(), info->rtt,
      info->cpu_usage, info->cpu_cores, info->frequency, info->bandwidth);

  int len = monitor_manager.GetCount();
  int index = -1;
  Monitor* p = NULL;
  for (int i = 0; i < len; i++) {
    p = monitor_manager.GetAt(i);

    if ((p != NULL) &&
        !strncmp(p->id, info->id.c_str(), info->id.length())) {
      DPRINT(CONN, DEBUG_INFO, "Found\n");
      index = i;
      break;
    }
  }

  if (index >= 0) {
    if (p->client != NULL) {
      p->client->Stop();
    }

    ServiceProvider* sp = CSTI<ServiceProvider>::getInstancePtr();
    sp->UpdateServiceInfo(sp->GenerateKey(p->address, p->service_port), info);

    monitor_manager.DelAt(index);
  }
}

static void OnDiscoveryClientEvent(int wParam,
                                   int lParam,
                                   void* pData,
                                   void* pParent) {
  discoveryInfo_t* pinfo = (discoveryInfo_t*)pData;

  CSTI<ServiceProvider>::getInstancePtr()->AddServiceInfo(
      pinfo->address, pinfo->service_port, pinfo->monitor_port);

  DPRINT(CONN, DEBUG_INFO, "OnDiscoveryClientEvent : (%d)-(%d)-(%s)\n",
         pinfo->service_port, pinfo->monitor_port, pinfo->address);
}

static void RequestRunService(DBusMessage* msg, DBusConnection* conn,
                              CServiceClient* service_client,
                              CNetTunProc* pTunClient) {
  dbus_bool_t stat = FALSE;
  std::string command_line;

  // Get command line arguments
  DBusMessageIter iter;
  dbus_message_iter_init(msg, &iter);
  if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
    DBusMessageIter sub;
    dbus_message_iter_recurse(&iter, &sub);

    while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
      const char* value;
      dbus_message_iter_get_basic(&sub, &value);
      command_line.append(value);
      command_line.append("&");
      dbus_message_iter_next(&sub);
    }
  }

  if (command_line.length())
    command_line.erase(command_line.end() -1);

  std::string message_string("service-request://");
  message_string += command_line;

  char* message = (char*) malloc(message_string.length() + 1);
  if (message) {
    memset(message, 0, message_string.length() + 1);
    strncpy(message, message_string.c_str(), message_string.length());

    ServiceProvider* ic = CSTI<ServiceProvider>::getInstancePtr();
    if (ic->Count() > 0) {
      ServiceInfo* info = ic->ChooseBestService();

      service_client->DataSend(message, strlen(message) + 1,
                                           info->address, info->service_port);
      RAW_PRINT("Request to run service is sent\n");
      stat = TRUE;
    } else if (pTunClient->HasTarget()) {
      unsigned long addr = pTunClient->GetTarget();
      if (addr) {
        //TODO(Hyunduk Kim) - Remove hardcoded port
        service_client->DataSend(message, strlen(message) + 1,
                                 U::CONV(addr), 9191);
        RAW_PRINT("Presence Service: Request %s to run service\n", U::CONV(addr));
        stat = TRUE;
      }
    }
    SAFE_FREE(message);
  }

  // Create a reply from the message
  DBusMessage* reply = dbus_message_new_method_return(msg);

  // Add the arguments to the reply
  DBusMessageIter reply_iter;
  dbus_message_iter_init_append(reply, &reply_iter);
  if (!dbus_message_iter_append_basic(&reply_iter, DBUS_TYPE_BOOLEAN, &stat)) {
    RAW_PRINT("Out Of Memory!\n");
  }

  // Send the reply and flush the connection
  if (!dbus_connection_send(conn, reply, NULL)) {
    RAW_PRINT("Fail to send the reply!\n");
    dbus_message_unref(reply);
    return;
  }

  dbus_connection_flush(conn);

  dbus_message_unref(reply);
}

int main(int argc, char** argv) {
  CbINIParser settings;
  int ret;
  const char* multicast_addr = NULL;
  const char* presence_addr = NULL;
  int multicast_port, presence_port;
  bool is_daemon;
  bool with_presence = true;;
  std::string multicast_addr_string;
  std::string presence_addr_string;
  ret = settings.Parse("client.ini");
  if (ret == -1)
    ret = settings.Parse("/usr/bin/client.ini");

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
  } else {
    RAW_PRINT("ini parse error(%d)\n", ret);
    if (argc < 3) {
      RAW_PRINT("Too Few Argument!!\n");
      RAW_PRINT("usage : %s mc_addr mc_port <presence> <pr_addr> <pr_port> "
                "<daemon>\n", argv[0]);
      RAW_PRINT("comment: mc(multicast),\n");
      RAW_PRINT("         presence (default is 0. This need to come with "
                "pr_addr and pr_port once you use it)\n");
      RAW_PRINT("         daemon (default is 0. You can use it if you want\n");
      return 0;
    }
    multicast_addr = argv[1];
    multicast_port = atoi(argv[2]);
    is_daemon = (argc == 4 && (strncmp(argv[3], "daemon", 6) == 0)) ||
                (argc == 7 && (strncmp(argv[6], "daemon", 6) == 0));
    with_presence = (argc >= 6 && (strncmp(argv[3], "presence", 8) == 0));
    if (with_presence) {
      presence_addr = argv[4];
      presence_port = atoi(argv[5]);
    }
  }

  if (is_daemon) {
    __OSAL_DaemonAPI_Daemonize("client-runner");
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  // Initialise the errors
  DBusError err;
  dbus_error_init(&err);

  // Connect to the bus
  DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    RAW_PRINT("Connection error (%s)\n", err.message);
    dbus_error_free(&err);
  }
  if (conn == NULL) {
    return 1;
  }

  // Request a name on the bus
  ret = dbus_bus_request_name(conn, "discovery.client.listener",
                              DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (dbus_error_is_set(&err))                               {
    RAW_PRINT("Name error (%s)\n", err.message);
    dbus_error_free(&err);
  }
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    return 1;
  }

  CSTI<CbDispatcher>::getInstancePtr()->Initialize();

  CDiscoveryClient* handle_discovery_client = new CDiscoveryClient(UUIDS_SDC);

  if (!handle_discovery_client->StartClient()) {
    RAW_PRINT("cannot start client\n");
    return 0;
  }

  CbMessage* mh_discovery_client = GetThreadMsgInterface(UUIDS_SDC);

  CSTI<CbDispatcher>::getInstancePtr()->Subscribe(DISCOVERY_RESPONSE_EVENT,
                                                  (void*)mh_discovery_client,
                                                  OnDiscoveryClientEvent);

  CServiceClient* handle_service_client = new CServiceClient(UUIDS_SRC);
	if (!handle_service_client->StartClient()) {
		RAW_PRINT("Cannot start service client\n");
		return 0;
	}

  CNetTunProc* pTunClient = NULL;
  if (with_presence) {
    pTunClient =
        new CNetTunProc("tunprocess", (char*)presence_addr, presence_port,
                        10240, 10000, 1000, 3);
    pTunClient->SetRole(CRouteTable::BROWSER);
    pTunClient->Create();
  }

  INT32 sequence_id = 0;
  DBusMessage* msg = NULL;
  while (true) {
    // Non blocking read of the next available message
    dbus_connection_read_write(conn, 0);
    msg = dbus_connection_pop_message(conn);

    // Check this is a method call for the right interface and method
    if (msg != NULL) {
      if (dbus_message_is_method_call(msg, "discovery.client.interface",
          "RunService")) {
        RequestRunService(msg, conn, handle_service_client, pTunClient);
      }
      // Free the message
      dbus_message_unref(msg);
    }

    sequence_id++;
    char message[] = "QUERY-SERVICE";
    handle_discovery_client->DataSend(message, strlen(message) + 1,
		                                  multicast_addr, multicast_port);
    __OSAL_Sleep(1000);

    ServiceProvider* sp = CSTI<ServiceProvider>::getInstancePtr();
    int num = sp->Count();
    for (int i = 0; i < num; i++) {
      ServiceInfo* info = sp->GetServiceInfo(i);
      RAW_PRINT("==Dump Service Provider Information!!==\n");
      RAW_PRINT("address : %s\n", info->address);
      RAW_PRINT("service port : %d\n", info->service_port);
      RAW_PRINT("monitor port : %d\n", info->monitor_port);
      RAW_PRINT("=======================================\n");

      Monitor* meta = new Monitor;
      INT32 magic = sequence_id * 100 + i;
      sprintf(meta->id, UUIDS_MDC, magic);
      strncpy(meta->address, info->address, strlen(info->address));
      meta->service_port = info->service_port;
      meta->monitor_port = info->monitor_port;

      meta->client = new MonitorClient(meta->id);
      monitor_manager.AddTail(meta);

      CbMessage* monitor_msg = GetThreadMsgInterface(meta->id);
      CSTI<CbDispatcher>::getInstancePtr()->Subscribe(
          MONITOR_RESPONSE_EVENT, (void*)monitor_msg, OnMonitorClientEvent);
      meta->client->Start(info->address, info->monitor_port);
      CHAR* monitor_packet = const_cast<CHAR*>("QUERY-MONITORING");
      meta->client->DataSend(monitor_packet, strlen(monitor_packet) + 1);
    }

    if (is_daemon) {
      if (__OSAL_DaemonAPI_IsRunning() != 1) {
        break;
      }
    } else {
      RAW_PRINT("Menu -- (Q) : quit program, (C) : continue\n");
      CHAR ch = getchar();
      if (ch == 'Q') {
        RAW_PRINT("Quit Program\n");
        break;
      } else if (ch == 'C') {
        continue;
      }
    }
  }

  dbus_error_free(&err);
  dbus_connection_unref(conn);

  handle_discovery_client->Close();

  SAFE_DELETE(pTunClient);
  return 0;
}
