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
// File.cpp: implementation of the CFile class.
//
//////////////////////////////////////////////////////////////////////

#include <sys/stat.h>
#include "Debugger.h"
#include "bFile.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
using namespace mmBase;

/**
 * @brief   File operation Wrapper class 생성자
 * @remarks       File operation Wrapper class 생성자
 * @param szFilePath
 * @return
*/
CbFile::CbFile(const CHAR* psz_file_path) {
  m_pHandle = NULL;
  ///< 생성자 에서는 open할 file의 path를 저장한다.
  memset(m_szFullPath, 0, sizeof(CHAR) * MAX_PATH);
  strncpy(m_szFullPath, psz_file_path,
          strlen(psz_file_path) > MAX_PATH ? MAX_PATH : strlen(psz_file_path));
}

/**
 * @brief   File operation wrapper class 소멸자
 * @remarks       File operation wrapper class 소멸자
 * @param none
 * @return
*/
CbFile::~CbFile() {
  Close();
  m_pHandle = NULL;
}

/**
 * @brief   File Handle Open
 * @remarks      File Handle Open
 * @param szMode
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::Open(const CHAR* szMode) {
  FILE_ERRORCODE iRc;
  m_pHandle = fopen(m_szFullPath, szMode);
  if (m_pHandle == NULL) {
    DPRINT(COMM, DEBUG_ERROR, "Could not open file --%s\n", m_szFullPath);
    iRc = FILEOP_ERR_OPEN;
  } else
    iRc = FILEOP_SUCCESS;
  return iRc;
}

/**
 * @brief   File Handle Close
 * @remarks       File Handle Close
 * @param none
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::Close() {
  FILE_ERRORCODE iRc;
  if (m_pHandle) {
    INT32 iRet = fclose(m_pHandle);
    if (iRet) {
      DPRINT(COMM, DEBUG_ERROR, "Could not close file --%s\n", m_szFullPath);
      iRc = FILEOP_ERR_CLOSE;
    } else {
      DPRINT(COMM, DEBUG_ERROR, "Closing media file succuss--%s\n",
             m_szFullPath);
      iRc = FILEOP_SUCCESS;
      m_pHandle = NULL;
    }
  } else {
    // DPRINT(COMM,DEBUG_ERROR,"file is not opened--%s\n",m_szFullPath);
    iRc = FILEOP_ERR_CLOSE;
  }
  return iRc;
}

/**
 * @brief   file peek
 * @remarks File의 Position은 변경하지 않고 iLen의 size 만큼 pByteRead buffer에
 * 채운다.
 * @param *pBuffer
 * @param iLen
 * @param pByteRead
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::Peek(UCHAR* pBuffer,
                                    INT32 iLen,
                                    INT32* pByteRead) {
  FILE_ERRORCODE iRc;
  INT32 iPos, iRead;
  iRc = GetPos(&iPos);
  if (iRc == FILEOP_SUCCESS) {
    iRc = Read(pBuffer, iLen, &iRead);
    if (iRc == FILEOP_SUCCESS) {
      *pByteRead = iRead;
      iRc = FILEOP_SUCCESS;
      iRc = SetPos(iPos, FILE_SEEK_BEGIN);
    }
  }
  return iRc;
}

/**
 * @brief   file read
 * @remarks iLen size 만큼의 data를 읽어 pBufer에 저장한다. file의 posion은 읽은
 * size만큼 이동한다.
 * @param *pBuffer
 * @param iLen
 * @param pByteRead
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::Read(UCHAR* pBuffer,
                                    INT32 iLen,
                                    INT32* pByteRead) {
  FILE_ERRORCODE iRc;
  if (!m_pHandle)
    return FILEOP_ERR_READ;

  //	_ASSERT(m_pHandle);
  //	_ASSERT(pBuffer);

  if (iLen <= 0) {
    DPRINT(COMM, DEBUG_ERROR, "The Byte to Read is smaller than 0!!\n");
    iRc = FILEOP_ERR_ARGUMENT;
    return iRc;
  }

  INT32 iRead = fread(pBuffer, sizeof(CHAR), iLen, m_pHandle);

  if (iRead != iLen) {
    if (feof(m_pHandle)) {
      DPRINT(COMM, DEBUG_ERROR, "File reaches end of file --%s\n",
             m_szFullPath);
      *pByteRead = iRead;
      iRc = FILEOP_ERR_EOF;
    } else {
      if (ferror(m_pHandle)) {
        DPRINT(COMM, DEBUG_ERROR, "File operation error occurs --%s\n",
               m_szFullPath);
        iRc = FILEOP_ERR_READ;
      } else {
        DPRINT(COMM, DEBUG_ERROR,
               "File operation is not complete but not error ???? --%s\n",
               m_szFullPath);
        iRc = FILEOP_ERR_UNKNOWN;
      }
    }
  } else {
    *pByteRead = iRead;
    iRc = FILEOP_SUCCESS;
  }
  return iRc;
}

/**
 * @brief   File write
 * @remarks File에 전달된 buffer의 data를 write한다.
 * @param *pData
 * @param iLen
 * @param pByteWritten
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::Write(const UCHAR* pData,
                                     INT32 iLen,
                                     INT32* pByteWritten) {
  FILE_ERRORCODE iRc;
  if (!m_pHandle)
    return FILEOP_ERR_WRITE;
  //	_ASSERT(m_pHandle);      // Make sure that the file has been opened
  //	_ASSERT(pData);
  if (iLen <= 0) {
    DPRINT(COMM, DEBUG_ERROR, "The Byte to Write is smaller than 0!!\n");
    iRc = FILEOP_ERR_ARGUMENT;
    return iRc;
  }

  INT32 iWritten = fwrite(pData, sizeof(CHAR), iLen, m_pHandle);
  if (iWritten != iLen) {
    if (feof(m_pHandle)) {
      DPRINT(COMM, DEBUG_ERROR, "File Reaches End of File--%s\n", m_szFullPath);
      *pByteWritten = iWritten;
      iRc = FILEOP_ERR_EOF;
    } else {
      if (ferror(m_pHandle)) {
        DPRINT(COMM, DEBUG_ERROR, "File operation error occurs --%s\n",
               m_szFullPath);
        iRc = FILEOP_ERR_WRITE;
      } else {
        DPRINT(COMM, DEBUG_ERROR,
               "File operation is not complete but not error ???? --%s\n",
               m_szFullPath);
        iRc = FILEOP_ERR_UNKNOWN;
      }
    }

  } else {
    *pByteWritten = iWritten;
    iRc = FILEOP_SUCCESS;
  }
  return iRc;
}

/**
 * @brief   file의 positon 획득
 * @remarks 현재 파일의 operation position을 return한다.
 * @param pPos
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::GetPos(INT32* pPos) {
  FILE_ERRORCODE iRc;
  if (!m_pHandle)
    return FILEOP_ERR_GETPOS;
  //	_ASSERT(m_pHandle);
  INT32 iPos = ftell(m_pHandle);
  if (iPos < 0) {
    DPRINT(COMM, DEBUG_ERROR, "File [ftell] operation error occurs --%s\n",
           m_szFullPath);
    iRc = FILEOP_ERR_GETPOS;
  } else {
    iRc = FILEOP_SUCCESS;
    *pPos = iPos;
  }
  return iRc;
}

/**
 * @brief   file position 설정
 * @remarks 현재의 file position을 다시 설정한다.
 * @param iLen
 * @param from
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::SetPos(INT32 iLen, FPOS_BASE from) {
  FILE_ERRORCODE iRc;
  if (!m_pHandle)
    return FILEOP_ERR_SETOPS;
  //	_ASSERT(m_pHandle);

  INT32 seek_entry;
  switch (from) {
    case FILE_SEEK_BEGIN:
      seek_entry = SEEK_SET;
      break;
    case FILE_SEEK_CURRENT:
      seek_entry = SEEK_CUR;
      break;
    case FILE_SEEK_END:
      seek_entry = SEEK_END;
      break;
    default:
      seek_entry = SEEK_SET;
      break;
  }
  INT32 iPos = fseek(m_pHandle, iLen, seek_entry);
  if (iPos < 0) {
    DPRINT(COMM, DEBUG_ERROR, "File [fseek] operation error occurs --%s\n",
           m_szFullPath);
    iRc = FILEOP_ERR_SETOPS;
  } else
    iRc = FILEOP_SUCCESS;
  return iRc;
}

/**
 * @brief    	get file size
 * @remark	현재의 file position을 다시 설정한다.
 * @param 	pSize
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::GetSize(UINT32* pSize) {
  FILE_ERRORCODE iRc;
  struct stat stFile;
  if (stat(m_szFullPath, &stFile) == 0) {
    *pSize = stFile.st_size;
    iRc = FILEOP_SUCCESS;
  } else {
    DPRINT(COMM, DEBUG_ERROR, "File [stat] operation error occurs --%s\n",
           m_szFullPath);
    iRc = FILEOP_ERR_GETSIZE;
  }
  return iRc;
}

/**
 * @brief   현재 open된 file의 handle을 return 한다.
 * @remark	현재 open된 file의 handle을 return 한다.
 * @param pHandle
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::GetHandle(PFHANDLE* pHandle) {
  FILE_ERRORCODE iRc;
  if (m_pHandle) {
    *pHandle = m_pHandle;
    iRc = FILEOP_SUCCESS;
  } else
    iRc = FILEOP_ERR_GETHADNLE;
  return iRc;
}

/**
 * @brief   현재 file의 path를 return 한다.
 * @remark	현재 file의 path를 return 한다.
 * @param szName
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::GetName(CHAR** szName) {
  FILE_ERRORCODE iRc;
  if (strlen(m_szFullPath) == 0)
    iRc = FILEOP_ERR_GETNAME;
  else {
    *szName = m_szFullPath;
    iRc = FILEOP_SUCCESS;
  }
  return iRc;
}

/**
 * @brief   전달된 path에 file이 실제로 존재하는지 check한다.
 * @remark  전달된 path에 file이 실제로 존재하는지 check한다.
 * @param bResult
 * @return FILE_ERRORCODE
*/
CbFile::FILE_ERRORCODE CbFile::Check() {
  FILE_ERRORCODE iRc;
  FILE* pfile = fopen(m_szFullPath, FILE_OPMODE_READ);
  if (pfile) {
    fclose(pfile);
    iRc = FILEOP_SUCCESS;
  } else {
    DPRINT(COMM, DEBUG_ERROR, "no File Exist --%s\n", m_szFullPath);
    iRc = FILEOP_ERR_NOFILE;
  }
  return iRc;
}

/**
 * @brief   file operation이 실패했을 경우 display를 위한 error string을
 * build한다.
 * @remark  file operation이 실패했을 경우 display를 위한 error string을
 * build한다.
 * @param err
 * @return CHAR*
*/
const CHAR* CbFile::MAKE_ERR_STRING(FILE_ERRORCODE err) {
  const CHAR* szErrString;
  switch (err) {
    case FILEOP_SUCCESS:
      szErrString = SZ_FILEOP_SUCCESS;
      break;
    case FILEOP_ERR_OPEN:
      szErrString = SZ_FILEOP_ERR_OPEN;
      break;
    case FILEOP_ERR_CLOSE:
      szErrString = SZ_FILEOP_ERR_CLOSE;
      break;
    case FILEOP_ERR_READ:
      szErrString = SZ_FILEOP_ERR_READ;
      break;
    case FILEOP_ERR_WRITE:
      szErrString = SZ_FILEOP_ERR_WRITE;
      break;
    case FILEOP_ERR_SETOPS:
      szErrString = SZ_FILEOP_ERR_SETOPS;
      break;
    case FILEOP_ERR_GETPOS:
      szErrString = SZ_FILEOP_ERR_GETPOS;
      break;
    case FILEOP_ERR_GETSIZE:
      szErrString = SZ_FILEOP_ERR_GETSIZE;
      break;
    case FILEOP_ERR_GETHADNLE:
      szErrString = SZ_FILEOP_ERR_GETHADNLE;
      break;
    case FILEOP_ERR_EOF:
      szErrString = SZ_FILEOP_ERR_EOF;
      break;
    default:
      szErrString = "";
      break;
  }
  return szErrString;
}

CbFile::FILE_ERRORCODE mmBase::MoveFile(CHAR* src, CHAR* dst) {
  return CbFile::FILEOP_SUCCESS;
}
CbFile::FILE_ERRORCODE mmBase::CopyFile(CHAR* src, CHAR* dst) {
  return CbFile::FILEOP_SUCCESS;
}
CbFile::FILE_ERRORCODE mmBase::DelFile(CHAR* target) {
  return CbFile::FILEOP_SUCCESS;
}
