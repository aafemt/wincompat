#pragma once

#include <stdlib.h>
#include <sys/inotify.h>

#include "winbase.h"
#include "fileapi.h"
#include "libloaderapi.h"
#include "winerror.h"
#include "windef.h"
#include "winnls.h"
#include "winuser.h"
#include "wincon.h"

#ifndef WIN32_LEAN_AND_MEAN
#include "rpc.h"
#endif // WIN32_LEAN_AND_MEAN

/* Windows compatibility header */

// This bit flag has no use on Linux, so define it to zero to make it no-op
const int O_BINARY = 0;

HANDLE CreateEvent(void*, WINBOOL bManualReset, WINBOOL bInitialState, char*);
WINBOOL SetEvent(HANDLE event);
WINBOOL ResetEvent(HANDLE event);

// Windows value for this constand is too "usual". Let's choose something with smaller probability of conflicts.
static const DWORD STILL_ACTIVE = ~0xDEAD;

WINBOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
WINBOOL TerminateThread(HANDLE hThread, DWORD dwExitCode);

const int INFINITE = -1;
const DWORD WAIT_FAILED = (DWORD)(-1);
// These values match those in MSDN. Perhaps, I can use own rules and expand this functionality a bit.
const DWORD WAIT_TIMEOUT = 258;
const DWORD WAIT_OBJECT_0 = 0;

// On Windows it has DWORD parameter, but changing of it to int makes use of INFINITY reliable
void Sleep(int Milliseconds);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WaitForMultipleObjects(DWORD nCount, HANDLE* lpHandles, WINBOOL bWaitAll, DWORD dwMilliseconds);


const DWORD SERVICE_STOPPED             = 1;
const DWORD SERVICE_START_PENDING       = 2;
const DWORD SERVICE_STOP_PENDING        = 3;
const DWORD SERVICE_RUNNING             = 4;
const DWORD SERVICE_CONTINUE_PENDING    = 5;
const DWORD SERVICE_PAUSE_PENDING       = 6;
const DWORD SERVICE_PAUSED              = 7;

typedef struct inotify_event FILE_NOTIFY_INFORMATION;
typedef FILE_NOTIFY_INFORMATION* PFILE_NOTIFY_INFORMATION;

// Following function is not a part of Win32 API
// It takes ownership of POSIX file handle and return Windows HANDLE.
HANDLE CreateFileHandle(int handle);
