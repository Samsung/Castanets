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

#include <string>

class ServerRunner {
 public:
  struct ServerRunnerParams {
    std::string multicast_addr;
    int multicast_port;
    int service_port;
    std::string exec_path;
    int monitor_port;
    bool with_presence;
    std::string presence_addr;
    int presence_port;
    bool is_daemon;
  };

  explicit ServerRunner(ServerRunnerParams& params);
  ~ServerRunner();

  int Initialize();
#if defined(WIN32)&& defined(RUN_AS_SERVICE)
  int Run(HANDLE ev_term);
#else
  int Run();
#endif
  void Stop();

 private:
  ServerRunnerParams params_;
  bool keep_running_;

  ServerRunner(const ServerRunner&) = delete;
  ServerRunner& operator=(const ServerRunner&) = delete;
};
