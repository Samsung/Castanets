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

#include "ioAPI.h"
#include "posixAPI.h"
#include "bDataType.h"
#include "bGlobDef.h"
#include "Debugger.h"

BOOL __OSAL_IOAPI_Init() {
  return TRUE;
}

BOOL __OSAL_IOAPI_DeInit() {
  return TRUE;
}

/**
 * @brief        device node open
 * @remarks      device node open
 * @param        device    	device inode path
 * @param        opt    	rw option
 * @param        pHandle    	io open handle pointer
 * @return       OSAL_Posix_Return
 */
OSAL_IO_Return __OSAL_IO_Open(/*[IN]*/ char* device,
                              /*[IN]*/ int opt,
                              OSAL_IO_Handle* pHandle) {
#ifdef WIN32
  return OSAL_IO_Success;
#elif defined(LINUX)
  OSAL_IO_Handle handle = open(device, opt);
  if (handle < 0) {
    *pHandle = -1;
    return OSAL_IO_Error;
  } else {
    *pHandle = handle;
    return OSAL_IO_Success;
  }
#endif
}

/**
 * @brief        device node read
 * @remarks      device node read
 * @param        handle    	io handle
 * @param        buff    	data pointer
 * @param        toread    	to read
 * @param        preadbyte    	real read byte
 * @return       OSAL_Posix_Return
 */
OSAL_IO_Return __OSAL_IO_Read(/*[IN]*/ OSAL_IO_Handle handle,
                              /*[OUT]*/ char* buff,
                              /*[IN]*/ int toread,
                              /*[OUT]*/ int* preadbyte) {
#ifdef WIN32
  return OSAL_IO_Success;
#elif defined(LINUX)
  int readbyte = read(handle, buff, toread);
  if (readbyte < 0) {
    *preadbyte = -1;
    return OSAL_IO_Error;
  } else {
    *preadbyte = readbyte;
    return OSAL_IO_Success;
  }
#endif
}

/**
 * @brief        device node write
 * @remarks      device node write
 * @param        handle    	io handle
 * @param        buff    	data pointer
 * @param        towrite    	to write
 * @param        pwrittenbyte   real written byte
 * @return       OSAL_IO_Return
 */
OSAL_IO_Return __OSAL_IO_Write(/*[IN]*/ OSAL_IO_Handle handle,
                               /*[IN]*/ char* buff,
                               /*[IN]*/ int towrite,
                               /*[OUT]*/ int* pwrittenbyte) {
#ifdef WIN32
  return OSAL_IO_Success;
#elif defined(LINUX)
  int writebyte = write(handle, buff, towrite);
  if (writebyte < 0) {
    *pwrittenbyte = -1;
    return OSAL_IO_Error;

  } else {
    *pwrittenbyte = writebyte;
    return OSAL_IO_Success;
  }
#endif
}

/**
 * @brief        device node close
 * @remarks      device node close
 * @param        handle    	io handle
 * @return       OSAL_IO_Return
 */
OSAL_IO_Return __OSAL_IO_Close(/*[IN]*/ OSAL_IO_Handle handle) {
#ifdef WIN32
  return OSAL_IO_Success;
#elif defined(LINUX)
  close(handle);
  return OSAL_IO_Success;
#endif
}

void PrintIOAPI() {
  printf("PrintIOAPIPrintIOAPIPrintIOAPIPrintIOAPIPrintIOAPIPrintIOAPI\n");
}
