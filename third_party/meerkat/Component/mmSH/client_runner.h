/*
 * Copyright 2019 Samsung Electronics Co., Ltd
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

#include <memory>
#include <string>

#if defined(USE_DBUS)
#include <Ecore.h>
#include <dbus/dbus.h>
#endif

#include "bTask.h"

class CDiscoveryClient;
class CServiceClient;

using GetTokenFunc = std::string (*)();
using VerifyTokenFunc = bool (*)(const char*);

namespace mmBase {
class CbMessage;
}

#if defined(ENABLE_STUN)
class CNetTunProc;
#endif

class ClientRunner {
 public:
  struct ClientRunnerParams {
    std::string multicast_addr;
    int multicast_port;
    bool self_discovery_enabled;
    bool with_presence;
    std::string presence_addr;
    int presence_port;
    bool is_daemon;
    GetTokenFunc get_token;
    VerifyTokenFunc verify_token;
  };

  static bool BuildParams(const std::string& ini_path,
                          ClientRunnerParams& params);
  static bool BuildParams(int argc, char** argv, ClientRunnerParams& params);

  explicit ClientRunner(ClientRunnerParams& params);
  ~ClientRunner();

  int Initialize();
#if defined(WIN32) && defined(RUN_AS_SERVICE)
  int Run(HANDLE ev_term);
#else
  int Run();
#endif
  void Stop();
#if defined(USE_DBUS)
  void DBusMessageCallback();
#endif

 private:
  bool BeforeRun();
  void AfterRun();

#if defined(USE_DBUS)
  void InitDBusConnection();
  void FreeDBusConnection();
  void RunService(DBusMessage* msg);
  void GetDevicelist(DBusMessage* msg);
  void RequestService(DBusMessage* msg);
  void RequestServiceOnDevice(DBusMessage* msg);
  void ReadCapability(DBusMessage* msg);
#endif

  ClientRunnerParams params_;
  std::unique_ptr<CDiscoveryClient> discovery_client_;
  mmBase::CbMessage* discovery_client_message_ = nullptr;

#if defined(ENABLE_STUN)
  std::unique_ptr<CNetTunProc*> tun_client_;
#endif

#if defined(USE_DBUS)
  DBusConnection* conn_ = nullptr;
  Ecore_Fd_Handler* conn_fd_handler_ = nullptr;
#endif

  bool keep_running_;

  ClientRunner(const ClientRunner&) = delete;
  ClientRunner& operator=(const ClientRunner&) = delete;
};
