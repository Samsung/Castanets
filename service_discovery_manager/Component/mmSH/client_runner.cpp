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

#include "client_runner.h"

#include "discovery_client.h"
#include "Dispatcher.h"
#include "monitor_client.h"
#include "NetTunProc.h"
#include "netUtil.h"
#include "osal.h"
#include "service_client.h"
#include "service_provider.h"
#include "TPL_SGT.h"

#if defined(WIN32)
#include "spawn_controller.h"
#endif

#if defined(LINUX) && !defined(ANDROID)
#include <dbus/dbus.h>
#endif

using namespace mmBase;
using namespace mmProto;

#define UUIDS_SDC "sdc-0000"
#define UUIDS_MDC "mdc-00%d"
#define UUIDS_SRC "src-0000"

typedef struct Monitor_ {
  MonitorClient* client;
  CbMessage* message_handle;
  CHAR id[16];
  CHAR address[16];
  INT32 service_port;
  INT32 monitor_port;
} Monitor;

static CbList<Monitor> monitor_manager;

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

    if ((p != NULL) && !strncmp(p->id, info->id.c_str(), info->id.length())) {
      index = i;
      break;
    }
  }

   if (index >= 0) {
    if (p->client != NULL)
      p->client->Stop();

    CSTI<CbDispatcher>::getInstancePtr()->UnSubscribe(
          MONITOR_RESPONSE_EVENT, (void*)p->message_handle, OnMonitorClientEvent);

    ServiceProvider* sp = CSTI<ServiceProvider>::getInstancePtr();
    sp->UpdateServiceInfo(sp->GenerateKey(p->address, p->service_port), info);

    SAFE_DELETE(p->client);
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

#if defined(LINUX) && !defined(ANDROID)
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
      DPRINT(COMM, DEBUG_INFO, "Request to run service is sent\n");
      stat = TRUE;
    } else if (pTunClient && pTunClient->HasTarget()) {
      unsigned long addr = pTunClient->GetTarget();
      if (addr) {
        //TODO(Hyunduk Kim) - Remove hardcoded port
        service_client->DataSend(message, strlen(message) + 1,
                                 U::CONV(addr), 9191);
        DPRINT(COMM, DEBUG_INFO,
               "Presence Service: Request %s to run service\n", U::CONV(addr));
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
    DPRINT(COMM, DEBUG_ERROR, "Out Of Memory!\n");
  }

  // Send the reply and flush the connection
  if (!dbus_connection_send(conn, reply, NULL)) {
    DPRINT(COMM, DEBUG_ERROR, "Fail to send the reply!\n");
    dbus_message_unref(reply);
    return;
  }

  dbus_connection_flush(conn);

  dbus_message_unref(reply);
}
#endif  // defined(LINUX) && !defined(ANDROID)

ClientRunner::ClientRunner(ClientRunnerParams& params)
    : params_(params) {}

ClientRunner::~ClientRunner() {}

int ClientRunner::Initialize() {
  if (params_.is_daemon) {
    __OSAL_DaemonAPI_Daemonize("client-runner");
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  CSTI<CbDispatcher>::getInstancePtr()->Initialize();

  return 0;
}

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
int ClientRunner::Run(HANDLE ev_term) {
#else
int ClientRunner::Run() {
#endif
  CDiscoveryClient* handle_discovery_client = new CDiscoveryClient(UUIDS_SDC);

  if (!handle_discovery_client->StartClient()) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start discovery client\n");
    return 1;
  }

  CbMessage* mh_discovery_client = GetThreadMsgInterface(UUIDS_SDC);

  CSTI<CbDispatcher>::getInstancePtr()->Subscribe(DISCOVERY_RESPONSE_EVENT,
                                                  (void*)mh_discovery_client,
                                                  OnDiscoveryClientEvent);

  CServiceClient* handle_service_client = new CServiceClient(UUIDS_SRC);
  if (!handle_service_client->StartClient()) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start service client\n");
    return 1;
  }

  CNetTunProc* pTunClient = NULL;
  if (params_.with_presence) {
    pTunClient = new CNetTunProc(
        "tunprocess",
        const_cast<char*>(params_.presence_addr.c_str()),
        params_.presence_port,
        10240, 10000, 1000, 3);
    pTunClient->SetRole(CRouteTable::BROWSER);
    pTunClient->Create();
  }

  INT32 sequence_id = 0;

#if defined(LINUX) && !defined(ANDROID)
  // Initialise the errors
  DBusError err;
  dbus_error_init(&err);

  // Connect to the bus
  DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (conn == NULL) {
    if (dbus_error_is_set(&err)) {
      DPRINT(COMM, DEBUG_ERROR, "dbus connection error! (%s)\n", err.message);
      dbus_error_free(&err);
    } else {
      DPRINT(COMM, DEBUG_ERROR, "dbus connection error!\n");
    }
    return 1;
  }

  // Request a name on the bus
  int ret = dbus_bus_request_name(conn, "discovery.client.listener",
                                  DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    if (dbus_error_is_set(&err))                               {
      DPRINT(COMM, DEBUG_ERROR, "dbus request name error! (%s)\n", err.message);
      dbus_error_free(&err);
    } else {
      DPRINT(COMM, DEBUG_ERROR, "dbus request name error!\n");
    }
    return 1;
  }

  DBusMessage* msg = NULL;
#endif  // defined(LINUX) && !defined(ANDROID)

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
  while (WaitForSingleObject(ev_term, 0) != WAIT_OBJECT_0) {
#else
  while (true) {
#endif
    sequence_id++;
    char message[] = "QUERY-SERVICE";
    handle_discovery_client->DataSend(message, strlen(message) + 1,
		                                  params_.multicast_addr.c_str(),
                                      params_.multicast_port);
    __OSAL_Sleep(1000);

    ServiceProvider* sp = CSTI<ServiceProvider>::getInstancePtr();
    int num = sp->Count();
    for (int i = 0; i < num; i++) {
      ServiceInfo* info = sp->GetServiceInfo(i);

      Monitor* meta = new Monitor;
      INT32 magic = sequence_id * 100 + i;
      sprintf(meta->id, UUIDS_MDC, magic);
      strncpy(meta->address, info->address, sizeof(meta->address));
      meta->service_port = info->service_port;
      meta->monitor_port = info->monitor_port;

      meta->client = new MonitorClient(meta->id);
      if (meta->client->Start(info->address, info->monitor_port)) {
        meta->message_handle = GetThreadMsgInterface(meta->id);
        CSTI<CbDispatcher>::getInstancePtr()->Subscribe(
            MONITOR_RESPONSE_EVENT, (void*)meta->message_handle, OnMonitorClientEvent);
        monitor_manager.AddTail(meta);

        CHAR* monitor_packet = const_cast<CHAR*>("QUERY-MONITORING");
        meta->client->DataSend(monitor_packet, strlen(monitor_packet) + 1);
      } else {
	SAFE_DELETE(meta->client);
        SAFE_DELETE(meta);
      }
    }
    sp->InvalidateServiceList();

#if defined(LINUX) && !defined(ANDROID)
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
#endif  // defined(LINUX) && !defined(ANDROID)

    if (params_.is_daemon) {
      if (__OSAL_DaemonAPI_IsRunning() != 1) {
        break;
      }
    }
  }

#if defined(LINUX) && !defined(ANDROID)
  dbus_error_free(&err);
  dbus_connection_unref(conn);
#endif

  handle_discovery_client->Close();

  SAFE_DELETE(pTunClient);
  return 0;
}
