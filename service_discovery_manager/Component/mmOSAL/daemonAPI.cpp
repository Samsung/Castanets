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

#include "daemonAPI.h"
#include "Debugger.h"

#if defined(LINUX) && !defined(ANDROID)
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static int running = 0;

#if defined(LINUX) && !defined(ANDROID)
static int pid_fd = -1;
static char pid_path[256] = { 0 };

static void handle_signal(int sig) {
  switch (sig) {
    case SIGHUP:
      /* Reload daemon */
      break;
    case SIGTERM:
      /* Exit daemon */
      if (pid_fd != -1) {
        lockf(pid_fd, F_ULOCK, 0);
        close(pid_fd);
        unlink(pid_path);
      }
      signal(SIGTERM, SIG_DFL);
      running = 0;
      break;
    default:
      /* Invalid signal */
      break;
  }
}
#endif

BOOL __OSAL_DaemonAPI_Init() {
  DPRINT(COMM, DEBUG_INFO, "[OSAL] DaemonAPI Initialize\n");
  return TRUE;
}

BOOL __OSAL_DaemonAPI_DeInit() {
  DPRINT(COMM, DEBUG_INFO, "[OSAL] DaemonAPI DeInitialize\n");
  return TRUE;
}

VOID __OSAL_DaemonAPI_Daemonize(const char* name) {
#if defined(LINUX) && !defined(ANDROID)
  struct rlimit limit;
  unsigned int i;
  sigset_t sigset;
  pid_t pid;
  char str[256];
  struct sigaction new_action;

  /* Close all open file descriptors except standard input, output, and error */
  if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
    exit(EXIT_FAILURE);
  }

  for (i = 3; i < limit.rlim_cur; i++) {
    close(i);
  }

  /* Reset all signal handlers to their default. */
  for (i = 0; i < _NSIG; i++) {
    signal(i, SIG_DFL);
  }

  /* Reset the signal mask */
  sigemptyset(&sigset);
  if (sigprocmask(SIG_SETMASK, &sigset, NULL) != 0) {
    exit(EXIT_FAILURE);
  }

  /* Fork to create a background process */
  pid = fork();

  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* Create an independent session */
  if (setsid() < 0) {
    exit(EXIT_FAILURE);
  }

  /* Fork to ensure that the daemon can never re-acquire a terminal again */
  pid = fork();

  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* Connect /dev/null to standard input, output, and error*/
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);

  /* Reset the umask to 0 */
  umask(0);

  /* Change the current directory to the root directory */
  if (chdir("/") != 0) {
    exit(EXIT_FAILURE);
  }

  /* Write the daemon PID to a PID file */
  sprintf(pid_path, "/run/%s.pid", name);

  pid_fd = open(pid_path, O_RDWR | O_CREAT, 0640);

  if (pid_fd < 0) {
    exit(EXIT_FAILURE);
  }

  if (lockf(pid_fd, F_TLOCK, 0) < 0) {
    exit(EXIT_FAILURE);
  }

  sprintf(str, "%d\n", getpid());

  write(pid_fd, str, strlen(str));

  /* Handle SIGHUP, SIGTERM */
  new_action.sa_handler = handle_signal;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;

  sigaction(SIGHUP, &new_action, NULL);
  sigaction(SIGTERM, &new_action, NULL);

  running = 1;

  openlog(name, LOG_PID|LOG_CONS, LOG_DAEMON);

  syslog(LOG_INFO, "%s daemon is running", name);
#endif
}

BOOL __OSAL_DaemonAPI_IsRunning() {
  return running;
}
