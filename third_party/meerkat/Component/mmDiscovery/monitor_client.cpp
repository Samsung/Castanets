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

#include "monitor_client.h"

using namespace mmBase;
using namespace mmProto;

ClientSocket::ClientSocket(MonitorClient* parent)
    : CpTcpClient(),
      parent_(parent) {
}

ClientSocket::ClientSocket(MonitorClient* parent, const CHAR* id)
    : CpTcpClient(id),
      parent_(parent) {
  info_.id = std::string(id);
}

VOID ClientSocket::DataRecv(OSAL_Socket_Handle sock, const CHAR* addr,
    long port, CHAR* data, INT32 len) {
  if (parent_)
    parent_->ParseRtt();

  DPRINT(COMM, DEBUG_INFO,
         "Receive- from:[socket:%d] msg:[%s] (rtt : %.4lf)\n",
         sock, data, parent_->Rtt());

  if (GenerateInfo(data))
    CbMessage::Send(MONITOR_RESPONSE_EVENT, 0, 0, sizeof(info_), (void*)&info_);
}

bool ClientSocket::GenerateInfo(CHAR* data) {
  if (!parent_ || !data)
    return false;

  info_.rtt = parent_->Rtt();

  std::string full_str(data);
  std::string delimiter = ";";
  std::string sub_delimiter = "=";

  size_t pos = 0;
  std::string token;
  while ((pos = full_str.find(delimiter)) != std::string::npos) {
      token = full_str.substr(0, pos);
      size_t sub_pos = 0;
      std::string sub_token;
      std::string value;
      while ((sub_pos = token.find(sub_delimiter)) != std::string::npos) {
        sub_token = token.substr(0, sub_pos);
        value = token.substr(sub_pos+1, token.length());
        if (sub_token == "USAGE") {
          info_.cpu_usage = atof(value.c_str());
          break;
        } else if (sub_token == "CORES") {
          info_.cpu_cores = atoi(value.c_str());
          break;
        } else if (sub_token == "FREQ") {
          info_.frequency = atof(value.c_str());
          break;
        } else if (sub_token == "BANDWIDTH") {
          info_.bandwidth = atof(value.c_str());
          break;
        }
      }
      full_str.erase(0, pos + delimiter.length());
  }

  return true;
}

VOID ClientSocket::EventNotify(CbSocket::SOCKET_NOTIFYTYPE type) {
  DPRINT(COMM, DEBUG_INFO, "Get Notify - event[%d]\n", type);
}

MonitorClient::MonitorClient()
    : sock_(this),
      rtt_(INVALID_RTT) {
}

MonitorClient::MonitorClient(const CHAR* id)
    : sock_(this, id),
      rtt_(INVALID_RTT) {
}

MonitorClient::~MonitorClient() {
}

BOOL MonitorClient::Start(const CHAR* addr, int port, int read) {
  DPRINT(COMM, DEBUG_INFO, "start monitor client - connect to (%s)(%d)\n",
      addr, port);

  if (!sock_.Create())
    return FALSE;
  if (!sock_.Open(addr, port))
    return FALSE;
  if (!sock_.Start(read))
    return FALSE;

  // make ping cmd
  ping_ = "ping -i 0.2 -c 5 ";
  ping_ += addr;
  ping_ += " >| ping_result";

  return TRUE;
}

BOOL MonitorClient::Stop() {
  sock_.Stop();
  sock_.Close();

  return TRUE;
}

VOID MonitorClient::DataSend(CHAR* data, int len) {
  CheckRtt();
  sock_.DataSend(data, len);
}

VOID MonitorClient::CheckRtt() {
  if (!ping_.empty())
    ignore_result(system(ping_.c_str()));
}

VOID MonitorClient::ParseRtt() {
  char buffer[1024] = "";
  char skip[3] = "";
  bool check = false;
  FILE* file = nullptr;

  file = fopen("./ping_result", "r");
  if (file == nullptr) {
    DPRINT(COMM, DEBUG_ERROR, "failed fopen\n");
    return;
  }
  while (fscanf(file, " %1023s", buffer) == 1) {
    if (!strncmp(buffer, "min/avg/max/mdev", strlen("min/avg/max/mdev"))) {
      ignore_result(fscanf(file, " %2s", skip));
      ignore_result(fscanf(file, " %1023s", buffer));

      std::string full_str(buffer);
      std::string delimiter = "/";
      std::string sub_delimiter = "=";

      size_t pos = 0;
      std::string token;
      if ((pos = full_str.find(delimiter)) != std::string::npos) {
        full_str.erase(0, pos + delimiter.length());

        if ((pos = full_str.find(delimiter)) != std::string::npos) {
          token = full_str.substr(0, pos);
          rtt_ = atof(token.c_str());
          check = true;
        }
      }
      break;
    }
  }
  fclose(file);

  if (!check)
    rtt_ = INVALID_RTT;
}
