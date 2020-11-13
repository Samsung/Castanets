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

#include <vector>

#include "TPL_SGT.h"
#include "bINIParser.h"
#include "bMessage.h"
#include "discovery_client.h"
#include "monitor_client.h"
#include "osal.h"
#include "service_client.h"
#include "service_provider.h"
#include "string_util.h"

#if defined(ENABLE_STUN)
#include "NetTunProc.h"
#include "netUtil.h"
#endif

#if defined(WIN32)
#include "spawn_controller.h"
#endif

using namespace mmBase;
using namespace mmProto;

#define UUIDS_SDC "sdc-0000"
#define UUIDS_MDC "mdc-00%d"
#define UUIDS_SRC "src-0000"

#if defined(LINUX) && !defined(ANDROID)
static DBusConnection* InitDBusConnection() {
  // Initialise the errors
  DBusError err;
  dbus_error_init(&err);

  // Connect to the bus
  DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (!conn) {
    if (dbus_error_is_set(&err)) {
      DPRINT(COMM, DEBUG_ERROR, "dbus connection error! (%s)\n", err.message);
      dbus_error_free(&err);
    } else {
      DPRINT(COMM, DEBUG_ERROR, "dbus connection error!\n");
    }
    return nullptr;
  }

  // Request a name on the bus
  int ret = dbus_bus_request_name(conn, "discovery.client.listener",
                                  DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    if (dbus_error_is_set(&err)) {
      DPRINT(COMM, DEBUG_ERROR, "dbus request name error! (%s)\n", err.message);
      dbus_error_free(&err);
    } else {
      DPRINT(COMM, DEBUG_ERROR, "dbus request name error!\n");
    }
    dbus_connection_unref(conn);
    return nullptr;
  }

  return conn;
}
#endif  // defined(LINUX) && !defined(ANDROID)

// static
bool ClientRunner::BuildParams(const std::string& ini_path,
                               ClientRunnerParams& params) {
  CbINIParser settings;

  int ret = settings.Parse(ini_path);
  if (ret !=0) {
    DPRINT(COMM, DEBUG_ERROR, "ini parse error(%d)\n", ret);
    return false;
  }

  params.multicast_addr = settings.GetAsString("multicast", "address", "");
  params.multicast_port = settings.GetAsInteger("multicast", "port", -1);
  params.self_discovery_enabled =
      settings.GetAsBoolean("multicast", "self-discovery-enabled", false);
  params.presence_addr = settings.GetAsString("presence", "address", "");
  params.presence_port = settings.GetAsInteger("presence", "port", -1);
  params.with_presence = params.presence_addr.length() > 0 &&
                         params.presence_port > 0;
  params.is_daemon = settings.GetAsBoolean("run", "run-as-damon", false);

  return true;
}

// static
bool ClientRunner::BuildParams(int argc, char** argv,
                               ClientRunnerParams& params) {
  if (argc < 3) {
    DPRINT(COMM, DEBUG_ERROR, "Too Few Argument!!\n");
    DPRINT(COMM, DEBUG_ERROR, "usage : %s mc_addr mc_port"
           "<presence> <pr_addr> <pr_port> <daemon>\n", argv[0]);
    DPRINT(COMM, DEBUG_ERROR, "comment: mc(multicast),\n");
    DPRINT(COMM, DEBUG_ERROR, "         presence (default is 0. This need to"
           "come with pr_addr and pr_port once you use it)\n");
    DPRINT(COMM, DEBUG_ERROR, "         daemon (default is 0."
           "You can use it if you want\n");
    return false;
  }

  params.multicast_addr = std::string(argv[1]);
  params.multicast_port = atoi(argv[2]);
  params.is_daemon = (argc == 4 && (strncmp(argv[5], "daemon", 6) == 0)) ||
                     (argc == 7 && (strncmp(argv[8], "daemon", 6) == 0));
  params.with_presence = (argc >= 6 && (strncmp(argv[3], "presence", 8) == 0));
  if (params.with_presence) {
    params.presence_addr = std::string(argv[4]);
    params.presence_port = atoi(argv[5]);
  }

  return true;
}

ClientRunner::ClientRunner(ClientRunnerParams& params)
    : params_(params),
      keep_running_(true) {
}

ClientRunner::~ClientRunner() {
}

int ClientRunner::Initialize() {
  if (params_.is_daemon) {
    __OSAL_DaemonAPI_Daemonize("client-runner");
  }

  InitDebugInfo(FALSE);
  SetModuleDebugFlag(MODULE_ALL, TRUE);
  SetDebugLevel(DEBUG_INFO);
  SetDebugFormat(DEBUG_NORMAL);

  return 0;
}

#if defined(WIN32)&& defined(RUN_AS_SERVICE)
int ClientRunner::Run(HANDLE ev_term) {
#else
int ClientRunner::Run() {
#endif
  if (!BeforeRun())
    return 1;

  INT32 sequence_id = 0;
#if defined(WIN32)&& defined(RUN_AS_SERVICE)
  while (WaitForSingleObject(ev_term, 0) != WAIT_OBJECT_0) {
#else
  while (true) {
#endif
    sequence_id++;
    // Send service query via multicast.
    char message[] = "QUERY-SERVICE";
    discovery_client_->DataSend(message, strlen(message) + 1,
		                params_.multicast_addr.c_str(),
                                params_.multicast_port);
    __OSAL_Sleep(1000);

    // Check response for service query.
    MSG_PACKET packet;
    while (discovery_client_message_->Recv(&packet, MQWTIME_WAIT_NO) >= 0) {
      if (packet.id == DISCOVERY_RESPONSE_EVENT) {
        if (packet.len > 0 && packet.msgdata) {
          DPRINT(COMM, DEBUG_INFO, "Discovery response: (%s:%d)\n",
                 (char*)packet.msgdata, packet.lParam);
          free(packet.msgdata);
        }
      }
    }
    CSTI<ServiceProvider>::getInstancePtr()->InvalidateServiceList();

#if defined(LINUX) && !defined(ANDROID)
    // Handle all pending dbus messages.
    while (true) {
      // Non blocking read of the next available message
      dbus_connection_read_write(conn_, 0);
      DBusMessage* msg = dbus_connection_pop_message(conn_);
      if (msg == NULL)
        break;

      // Check this is a method call for the right interface and method
      if (dbus_message_is_method_call(msg, "discovery.client.interface",
                                      "RunService")) {
        RunService(msg);
      } else if (dbus_message_is_method_call(msg, "discovery.client.interface",
                                             "GetDevicelist")) {
        GetDevicelist(msg);
      } else if (dbus_message_is_method_call(msg, "discovery.client.interface",
                                             "RequestService")) {
        RequestService(msg);
      } else if (dbus_message_is_method_call(msg, "discovery.client.interface",
                                             "RequestServiceOnDevice")) {
        RequestServiceOnDevice(msg);
      } else if (dbus_message_is_method_call(msg, "discovery.client.interface",
                                             "ReadCapability")) {
        ReadCapability(msg);
      }

      // Free the message
      dbus_message_unref(msg);
    }
#endif

    if (!keep_running_)
      break;

    if (params_.is_daemon) {
      if (__OSAL_DaemonAPI_IsRunning() != 1) {
        break;
      }
    }
  }

  AfterRun();
  return 0;
}

void ClientRunner::Stop() {
  keep_running_ = false;
}

bool ClientRunner::BeforeRun() {
  CSTI<ServiceProvider>::getInstancePtr()->SetCallbacks(
      params_.get_token, params_.verify_token);

  discovery_client_.reset(
      new CDiscoveryClient(UUIDS_SDC, params_.self_discovery_enabled));

  if (!discovery_client_->StartClient()) {
    DPRINT(COMM, DEBUG_ERROR, "Cannot start discovery client\n");
    return false;
  }

  discovery_client_message_ = GetThreadMsgInterface(UUIDS_SDC);

#if defined(LINUX) && !defined(ANDROID)
  conn_ = InitDBusConnection();
  if (!conn_)
    return false;
#endif  // defined(LINUX) && !defined(ANDROID)

#if defined(ENABLE_STUN)
  if (params_.with_presence) {
    tun_client_.reset(
        new CNetTunProc("tunprocess",
                        const_cast<char*>(params_.presence_addr.c_str()),
                        params_.presence_port,
                        10240, 10000, 1000, 3));
    tun_client_->SetRole(CRouteTable::BROWSER);
    tun_client_->Create();
  }
#endif

  return true;
}

void ClientRunner::AfterRun() {
  discovery_client_->Close();

#if defined(LINUX) && !defined(ANDROID)
  if (conn_) {
    dbus_connection_unref(conn_);
    conn_ = nullptr;
  }
#endif

}

#if defined(LINUX) && !defined(ANDROID)
void ClientRunner::RunService(DBusMessage* msg) {
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
    command_line.erase(command_line.end() - 1);

  std::string message_string("service-request://");
  message_string += command_line;

  char* message = (char*)malloc(message_string.length() + 1);
  if (message) {
    mmBase::strlcpy(message, message_string.c_str(),
                    message_string.length() + 1);

    ServiceProvider* ic = CSTI<ServiceProvider>::getInstancePtr();
    if (ic->Count() > 0) {
      ServiceInfo* info = ic->ChooseBestService();
      if (info) {
        info->service_client->DataSend(message, strlen(message) + 1);
        DPRINT(COMM, DEBUG_INFO, "Request to run service is sent\n");
        stat = TRUE;
      }
    }
#if defined(ENABLE_STUN)
    else if (tun_client_ && tun_client_->HasTarget()) {
      unsigned long addr = tun_client->GetTarget();
      if (addr) {
        // TODO(Hyunduk Kim) - Remove hardcoded port
        /*
        service_client->DataSend(message, strlen(message) + 1,
                                 U::CONV(addr), 9191);
        DPRINT(COMM, DEBUG_INFO,
               "Presence Service: Request %s to run service\n",
               U::CONV(addr));
        stat = TRUE;
        */
      }
    }
#endif
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
  if (!dbus_connection_send(conn_, reply, NULL)) {
    DPRINT(COMM, DEBUG_ERROR, "Fail to send the reply!\n");
    dbus_message_unref(reply);
    return;
  }

  dbus_connection_flush(conn_);
  dbus_message_unref(reply);
}

void ClientRunner::GetDevicelist(DBusMessage* msg) {
  DPRINT(COMM, DEBUG_INFO, "%s()", __func__);
  // Get arguments
  char *service_name = NULL, *exec_type = NULL;

  DBusMessageIter iter;
  dbus_message_iter_init(msg, &iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &service_name);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &exec_type);
  dbus_message_iter_next(&iter);
  DPRINT(COMM, DEBUG_INFO, "%s() %s, %s", __func__, service_name,
         exec_type);

  std::vector<const char*> ipaddrs;
  ServiceProvider* ic = CSTI<ServiceProvider>::getInstancePtr();
  int cnt = ic->Count();
  for (int i = 0; i < cnt; i++) {
    if (ic->GetServiceInfo(i)->service_client->GetState() ==
        CServiceClient::CONNECTED) {
      ipaddrs.push_back(
          ic->GetServiceInfo(i)->service_client->GetServerAddress());
    }
  }

  // Create a reply from the message
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter reply_iter;
  dbus_message_iter_init_append(reply, &reply_iter);
  DBusMessageIter sub;
  bool success =
      dbus_message_iter_open_container(&reply_iter, DBUS_TYPE_ARRAY, "s", &sub);
  if (success) {
    for (const char* value : ipaddrs)
      dbus_message_iter_append_basic(&sub, DBUS_TYPE_STRING, &value);
    dbus_message_iter_close_container(&reply_iter, &sub);
  }

  // Send the reply and flush the connection
  if (!dbus_connection_send(conn_, reply, NULL)) {
    DPRINT(COMM, DEBUG_ERROR, "Fail to send the reply!\n");
  }

  dbus_connection_flush(conn_);
  dbus_message_unref(reply);
}

void ClientRunner::RequestService(DBusMessage* msg) {
  DPRINT(COMM, DEBUG_INFO, "%s()", __func__);
  // Get arguments
  const char* app_name = NULL;
  bool self_select = false;
  const char* exec_type = NULL;
  const char* exec_parameter = NULL;

  DBusMessageIter iter;
  dbus_message_iter_init(msg, &iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &app_name);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_BOOLEAN);
  dbus_message_iter_get_basic(&iter, &self_select);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &exec_type);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &exec_parameter);
  dbus_message_iter_next(&iter);
  DPRINT(COMM, DEBUG_INFO, "%s() %s, %d, %s, %s", __func__, app_name,
         self_select, exec_type, exec_parameter);

  // TODO : Implement functionality..
  int ret = 0;

  // Create a reply from the message
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter reply_iter;
  dbus_message_iter_init_append(reply, &reply_iter);
  dbus_message_iter_append_basic(&reply_iter, DBUS_TYPE_INT32, &ret);

  // Send the reply and flush the connection
  if (!dbus_connection_send(conn_, reply, NULL)) {
    DPRINT(COMM, DEBUG_ERROR, "Fail to send the reply!\n");
  }

  dbus_connection_flush(conn_);
  dbus_message_unref(reply);
}

void ClientRunner::RequestServiceOnDevice(DBusMessage* msg) {
  DPRINT(COMM, DEBUG_INFO, "%s()", __func__);
  // Get arguments
  const char* app_name = NULL;
  bool self_select = false;
  const char* exec_type = NULL;
  const char* exec_parameter = NULL;
  const char* ip = NULL;

  DBusMessageIter iter;
  dbus_message_iter_init(msg, &iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &app_name);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_BOOLEAN);
  dbus_message_iter_get_basic(&iter, &self_select);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &exec_type);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &exec_parameter);
  dbus_message_iter_next(&iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &ip);
  dbus_message_iter_next(&iter);
  DPRINT(COMM, DEBUG_INFO, "%s() %s, %d, %s, %s, %s", __func__,
         app_name, self_select, exec_type, exec_parameter, ip);

  int ret = 0;
  std::string message_string("service-request://");
  message_string += std::string(exec_parameter);

  char* message = (char*)malloc(message_string.length() + 1);
  if (message) {
    mmBase::strlcpy(message, message_string.c_str(),
                    message_string.length() + 1);

    ServiceProvider* ic = CSTI<ServiceProvider>::getInstancePtr();
    if (ic) {
      ServiceInfo* info = ic->GetServiceInfo(ip);
      if (info) {
        info->service_client->DataSend(message, strlen(message) + 1);
        DPRINT(COMM, DEBUG_INFO, "RequestServiceOnDevice is sent\n");
        ret = 1;
      }
    }
    SAFE_FREE(message);
  }

  // Create a reply from the message
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter reply_iter;
  dbus_message_iter_init_append(reply, &reply_iter);
  dbus_message_iter_append_basic(&reply_iter, DBUS_TYPE_INT32, &ret);

  // Send the reply and flush the connection
  if (!dbus_connection_send(conn_, reply, NULL)) {
    DPRINT(COMM, DEBUG_ERROR, "Fail to send the reply!\n");
  }

  dbus_connection_flush(conn_);
  dbus_message_unref(reply);
}

void ClientRunner::ReadCapability(DBusMessage* msg) {
  DPRINT(COMM, DEBUG_INFO, "%s()", __func__);
  // Get arguments
  char* ip = NULL;

  DBusMessageIter iter;
  dbus_message_iter_init(msg, &iter);
  __ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING);
  dbus_message_iter_get_basic(&iter, &ip);
  dbus_message_iter_next(&iter);
  DPRINT(COMM, DEBUG_INFO, "%s() %s", __func__, ip);

  auto* info = CSTI<ServiceProvider>::getInstancePtr()->GetServiceInfo(ip);
  const char* result = info ? info->capability.c_str() : "";

  // Create a reply from the message
  DBusMessage* reply = dbus_message_new_method_return(msg);
  DBusMessageIter reply_iter;
  dbus_message_iter_init_append(reply, &reply_iter);
  dbus_message_iter_append_basic(&reply_iter, DBUS_TYPE_STRING, &result);

  // Send the reply and flush the connection
  if (!dbus_connection_send(conn_, reply, NULL)) {
    DPRINT(COMM, DEBUG_ERROR, "Fail to send the reply!\n");
  }

  dbus_connection_flush(conn_);
  dbus_message_unref(reply);
}

#endif
