#ifndef __INCLUDE_SPAWN_CONTROLLER_H__
#define __INCLUDE_SPAWN_CONTROLLER_H__

#include "TPL_SGT.h"

#if defined(WIN32)
#include <Windows.h>
#include <tchar.h>
#endif

#if defined(WIN32)
typedef int (*entryPoint)(HANDLE terminate, int count, char** argument_list);
#endif

class CSpawnController : public CSTI<CSpawnController> {
 public:
  CSpawnController();
  virtual ~CSpawnController();

  int ServiceRegister(entryPoint entry);
  static void OnExitProgram();
  static void WINAPI RunAsService(int argc, char** argv);
  static void WINAPI ServiceCtrlHandler(ULONG CtrlCode);
};

#endif  //__INCLUDE_INFORMATION_CONTAINER_H__
