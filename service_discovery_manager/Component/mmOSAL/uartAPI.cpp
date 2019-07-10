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

#include "uartAPI.h"
#include "Debugger.h"

#ifdef WIN32
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#elif defined(LINUX)
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

OSAL_Device_Handle __OSAL_Uart_Open(const char* node) {
  OSAL_Device_Handle hDev;
  struct termios UartAttr;
  hDev = open(node, O_RDWR | O_NONBLOCK | O_NOCTTY);
  if (hDev > 0) {
    bzero((void*)&UartAttr, sizeof(UartAttr));
    UartAttr.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    UartAttr.c_iflag = IGNPAR /*| ICRNL*/;
    UartAttr.c_oflag = OPOST;

    UartAttr.c_lflag = 0;
    UartAttr.c_cc[VTIME] = 0; /* .. ... timer. disable */
    UartAttr.c_cc[VMIN] = 1;  /* .. 1 .. .. ... blocking  */
    tcsetattr(hDev, TCSANOW, &UartAttr);
  }
  return hDev;
}

void __OSAL_Uart_Close(OSAL_Device_Handle hDev) {
  close(hDev);
}

int __OSAL_Uart_Write(OSAL_Device_Handle hDev, const char* buff, int len) {
  if (hDev > 0) {
    return write(hDev, buff, len);
  } else
    return -1;
}

int __OSAL_Uart_Read(OSAL_Device_Handle hDev, char* buff, int max_read) {
  if (hDev > 0)
    return read(hDev, buff, max_read);
  else
    return -1;
}

int __OSAL_Uart_Set(OSAL_Device_Handle hDev, u_attribute attr) {
  if (hDev > 0) {
    struct termios UartAttr;
    bzero((void*)&UartAttr, sizeof(UartAttr));
    UartAttr.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    UartAttr.c_iflag = IGNPAR /*| ICRNL*/;
    UartAttr.c_oflag = OPOST;

    UartAttr.c_lflag = attr.flag;
    UartAttr.c_cc[VTIME] = attr.vtime; /* .. ... timer. disable */
    UartAttr.c_cc[VMIN] = attr.vmin;   /* .. 1 .. .. ... blocking  */
    tcsetattr(hDev, TCSANOW, &UartAttr);
    return 0;
  } else
    return -1;
}

BOOL __OSAL_Uart_Init() {
  return TRUE;
}
BOOL __OSAL_Uart_DeInit() {
  return TRUE;
}
