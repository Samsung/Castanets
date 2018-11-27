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

#ifndef __INCLUDE_COMMON_FILE_H__
#define __INCLUDE_COMMON_FILE_H__

#include "Debugger.h"
#include "bGlobDef.h"
#include "bDataType.h"

#define MAX_ERROR_STRING 64

#define SZ_FILEOP_SUCCESS "FILE OPERATION -- NO ERROR"
#define SZ_FILEOP_ERR_OPEN "FILE OPERATION	-- **ERR** OPEN"
#define SZ_FILEOP_ERR_CLOSE "FILE OPERATION -- **ERR** CLOSE"
#define SZ_FILEOP_ERR_READ "FILE OPERATION -- **ERR** READ"
#define SZ_FILEOP_ERR_WRITE "FILE OPERATION -- **ERR** WRITE"
#define SZ_FILEOP_ERR_SETOPS "FILE OPERATION -- **ERR** SETPOS"
#define SZ_FILEOP_ERR_GETPOS "FILE OPERATION -- **ERR** GETPOS"
#define SZ_FILEOP_ERR_GETSIZE "FILE OPERATION -- **ERR** GETSIZE"
#define SZ_FILEOP_ERR_GETHADNLE "FILE OPERATION -- **ERR** GETHANDLE"
#define SZ_FILEOP_ERR_EOF "FILE OPERATION -- **ERR** REACH EOF"

#define FILE_OPMODE_READ "rb"
#define FILE_OPMODE_WRITE "wb"
#define FILE_OPMODE_RW "rw"
#define FILE_OPMODE_DEFAULT FILE_OPMODE_READ

namespace mmBase {

class CbFile {
 public:
  enum FILE_ERRORCODE {
    FILEOP_SUCCESS = 0,
    FILEOP_ERR_NOFILE,
    FILEOP_ERR_OPEN,
    FILEOP_ERR_CLOSE,
    FILEOP_ERR_READ,
    FILEOP_ERR_WRITE,
    FILEOP_ERR_SETOPS,
    FILEOP_ERR_GETPOS,
    FILEOP_ERR_GETSIZE,
    FILEOP_ERR_GETNAME,
    FILEOP_ERR_GETHADNLE,
    FILEOP_ERR_EOF,
    FILEOP_ERR_ARGUMENT,
    FILEOP_ERR_UNKNOWN
  };

  enum FPOS_BASE { FILE_SEEK_BEGIN = 0, FILE_SEEK_CURRENT, FILE_SEEK_END };

 public:
  CbFile(const CHAR* szFilePath);
  virtual ~CbFile();

  FILE_ERRORCODE Open(const CHAR* szMode = FILE_OPMODE_DEFAULT);
  FILE_ERRORCODE Close();
  FILE_ERRORCODE Peek(UCHAR* pBuffer, INT32 iLen, INT32* pByteRead);
  FILE_ERRORCODE Read(UCHAR* pBuffer, INT32 iLen, INT32* pByteRead);
  FILE_ERRORCODE Write(const UCHAR* pData, INT32 iLen, INT32* pByteWritten);
  FILE_ERRORCODE GetPos(INT32* pPos);
  FILE_ERRORCODE SetPos(INT32 iLen, FPOS_BASE from);
  FILE_ERRORCODE GetSize(UINT32* pSize);
  FILE_ERRORCODE GetHandle(PFHANDLE* pHandle);
  FILE_ERRORCODE GetName(CHAR** szName);

  FILE_ERRORCODE Check();

  const CHAR* MAKE_ERR_STRING(FILE_ERRORCODE err);

 private:
  CbFile(const CbFile&);  // mmBase:: class is not allowed instance copy and
                          // substitute
  CbFile& operator=(const CbFile&);  // do not implement this method. i
                                     // abnomally make link error when user
                                     // intend to copy and substitute

 protected:
 private:
  PFHANDLE m_pHandle;
  CHAR m_szFullPath[MAX_PATH];
};

CbFile::FILE_ERRORCODE MoveFile(CHAR* src, CHAR* dst);
CbFile::FILE_ERRORCODE CopyFile(CHAR* src, CHAR* dst);
CbFile::FILE_ERRORCODE DelFile(CHAR* target);
}
#endif
