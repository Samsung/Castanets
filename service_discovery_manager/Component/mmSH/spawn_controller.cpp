#include "Debugger.h"
#include "spawn_controller.h"

#include <tchar.h>

#if defined(WIN32)
SERVICE_STATUS        csm_service_status = { 0 };
entryPoint            csm_worker_entry = NULL;
SERVICE_STATUS_HANDLE csm_status_handle = NULL;
HANDLE                csm_service_stop_event = INVALID_HANDLE_VALUE;
#define CSM_SERVICE_NAME  "Catanets Service Manager"
#endif

CSpawnController::CSpawnController() {}

CSpawnController::~CSpawnController() {}

int CSpawnController::ServiceRegister(entryPoint entry) {
#if defined(WIN32)
  csm_worker_entry = entry;
  SERVICE_TABLE_ENTRY ServiceTable[] =
  {
    { (LPSTR)CSM_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)RunAsService },
    { NULL, NULL }
  };

  if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
  {
    RAW_PRINT("StartServiceCtrlDispatcher error\n");
    return GetLastError();
  }
#endif
  return 0;
}

void CSpawnController::OnExitProgram() {
  DPRINT(CONN, DEBUG_FATAL,
         "Fatal Error is occured -> Exit Program\n");
  exit(0);
}

void WINAPI CSpawnController::ServiceCtrlHandler(ULONG CtrlCode) {
#if defined(WIN32)
  RAW_PRINT("(CSM): Iterate service control Handler\n");

  switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
      RAW_PRINT("(CSM): SERVICE_CONTROL_STOP is Requested\n");
      if (csm_service_status.dwCurrentState != SERVICE_RUNNING)
        break;
      csm_service_status.dwControlsAccepted = 0;
      csm_service_status.dwCurrentState = SERVICE_STOP_PENDING;
      csm_service_status.dwWin32ExitCode = 0;
      csm_service_status.dwCheckPoint = 4;

      if (SetServiceStatus(csm_status_handle, &csm_service_status) == FALSE) {
        RAW_PRINT("(CSM): SetServiceStatus Failed!!!\n");
      }
      SetEvent(csm_service_stop_event);
      break;

    default:
      break;
  }
  RAW_PRINT("(CSM): Iterate service control Handler done \n");
#endif
}

void WINAPI CSpawnController::RunAsService(int argc, char** argv) {
#if defined(WIN32)
  DWORD Status = E_FAIL;
  RAW_PRINT("Run as service Entry !!!\n");
  csm_status_handle = RegisterServiceCtrlHandler(CSM_SERVICE_NAME, ServiceCtrlHandler);

  if (csm_status_handle == NULL) {
    OnExitProgram();
  }

  ZeroMemory(&csm_service_status, sizeof(csm_service_status));
  csm_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  csm_service_status.dwControlsAccepted = 0;
  csm_service_status.dwCurrentState = SERVICE_START_PENDING;
  csm_service_status.dwWin32ExitCode = 0;
  csm_service_status.dwServiceSpecificExitCode = 0;
  csm_service_status.dwCheckPoint = 0;

  if (SetServiceStatus(csm_status_handle, &csm_service_status) == FALSE) {
    RAW_PRINT("(CSM): SetServiceStatus returned error\n");
  }

  csm_service_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (csm_service_stop_event == NULL) {
    RAW_PRINT("(CSM): CreateEvent(service stop event) Failed !!!\n");
    csm_service_status.dwControlsAccepted = 0;
    csm_service_status.dwCurrentState = SERVICE_STOPPED;
    csm_service_status.dwWin32ExitCode = GetLastError();
    csm_service_status.dwCheckPoint = 1;
    if (SetServiceStatus(csm_status_handle, &csm_service_status) == FALSE) {
      RAW_PRINT("(CSM) : SetServiceStatus Failed !!!\n");
    }
    OnExitProgram();
  }
  csm_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;\
  csm_service_status.dwCurrentState = SERVICE_RUNNING;
  csm_service_status.dwWin32ExitCode = 0;
  csm_service_status.dwCheckPoint = 0;

  if (SetServiceStatus(csm_status_handle, &csm_service_status) == FALSE) {
    RAW_PRINT("(CSM) : SetServiceStatus Failed !!!\n");
  }
  csm_worker_entry(csm_service_stop_event, argc, argv);
  RAW_PRINT("(CSM) : Stop Event signaled !!!\n");
  CloseHandle(csm_service_stop_event);
	csm_service_status.dwControlsAccepted = 0;
  csm_service_status.dwCurrentState = SERVICE_STOPPED;
  csm_service_status.dwWin32ExitCode = 0;
  csm_service_status.dwCheckPoint = 3;

  if (SetServiceStatus(csm_status_handle, &csm_service_status) == FALSE) {
    RAW_PRINT("(CSM) : SetServiceStatus Failed!!!\n");
  }
  RAW_PRINT("(CSM): Performed Cleanup Operations\n");
#endif
}
