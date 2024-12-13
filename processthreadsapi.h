#pragma once

#include "windef.h"

DWORD GetCurrentProcessId();
DWORD GetCurrentThreadId();

inline HANDLE GetCurrentProcess()
{
	return (HANDLE)(-1);
}

WINBOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, HANDLE* TokenHandle);
