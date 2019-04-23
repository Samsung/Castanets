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

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef WIN32
#include <stdio.h>
#include <tchar.h>
#elif defined(LINUX)
#include <unistd.h>
#endif

#include "Debugger.h"
#include "processAPI.h"

#ifdef WIN32
HANDLE hchild_proc_stdin_r;
HANDLE hchild_proc_stdin_w;
HANDLE hchild_proc_stdout_r;
HANDLE hchild_proc_stdout_w;
#endif

BOOL __OSAL_Create_Child_Process(std::vector<char*>& argv, 
                                 OSAL_PROCESS_ID* ppid, 
                                 OSAL_PROCESS_ID* ptid){
#ifdef WIN32
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
  sa.bInheritHandle = TRUE; 
  sa.lpSecurityDescriptor = NULL;   

  CreatePipe(&hchild_proc_stdout_r, &hchild_proc_stdout_w, &sa, 0);
  SetHandleInformation(hchild_proc_stdout_r, HANDLE_FLAG_INHERIT, 0);
  
  CreatePipe(&hchild_proc_stdin_r, &hchild_proc_stdin_w, &sa, 0);
  SetHandleInformation(hchild_proc_stdin_w, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  ZeroMemory( &si, sizeof(si) );  
  si.cb = sizeof(si);
  si.hStdInput = hchild_proc_stdin_r;
  si.hStdError = si.hStdOutput = hchild_proc_stdout_w;
  si.dwFlags |= STARTF_USESTDHANDLES;

  ZeroMemory( &pi, sizeof(pi) );
  
  char cmdline[4096];
  ZeroMemory(cmdline, 4096);

  int index = 0;
  std::vector<char*>::iterator iter;
  for(iter = argv.begin(); iter != argv.end(); ++iter){
    if(iter != argv.begin()){
	  cmdline[index] = ' ';
      index++;
    }
    int l = strlen(*iter);
    strncpy(&cmdline[index], *iter, l);
    index += l;
  }

  if( !CreateProcess( NULL, (LPSTR)cmdline, NULL, NULL, FALSE, 0,              
                      NULL, NULL, &si, &pi )) 
  {
    printf( "CreateProcess failed (%d).\n", GetLastError() );
    return FALSE;
  }
  *ppid = pi.hProcess;
  *ptid = pi.hThread;

#elif defined(LINUX)
  pid_t pid = fork();
  if (pid == -1)
    return false;

  if (pid == 0) {
    execv(argv[0], argv.data());
  }

  *ppid = pid;
  *ptid = 0;
#endif
  return TRUE;
}

VOID __OSAL_Write_To_Pipe(char* std_in, int len) {
#ifdef WIN32
  DWORD dwWritten;    
  WriteFile(hchild_proc_stdin_w, std_in, len, &dwWritten, NULL);    
  CloseHandle(hchild_proc_stdin_w);
#endif
}

VOID __OSAL_Read_From_Pipe(char* std_out, int* len) {
#ifdef WIN32
  ReadFile( hchild_proc_stdout_r, std_out, 4096, (LPDWORD)len, NULL);
#endif
}