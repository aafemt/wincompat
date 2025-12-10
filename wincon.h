#pragma once

#include <signal.h>

#include "windef.h"

#define CTRL_C_EVENT SIGINT
#define CTRL_BREAK_EVENT SIGQUIT
#define CTRL_CLOSE_EVENT SIGHUP
#define CTRL_LOGOFF_EVENT SIGKILL // No such signal outside of X-Window but some constant must be assigned
#define CTRL_SHUTDOWN_EVENT SIGTERM

typedef BOOL (WINAPI *PHANDLER_ROUTINE)(DWORD CtrlType);

BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add);
BOOL GenerateConsoleCtrlEvent(DWORD dwCtrlEvent, DWORD dwProcessGroupId);
