/*******************************************************
*
*  Tests for synchronization functions
*
********************************************************/

#include <windows.h>
#include <process.h>

#include "globals.h"

bool eventArrived = false;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			eventArrived = true;
			return TRUE;
		default:
			// Pass other signals to the next handler.
			return FALSE;
	}
}

unsigned WINAPI Thread(void*)
{
	if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0))
		failure("GenerateConsoleCtrlEvent error %lu\n", GetLastError());
	return 0;
}

void RunSyncTests()
{
	if (SetConsoleCtrlHandler(CtrlHandler, TRUE) == 0)
	{
		failure("Error setting user-side Ctrl-C handler. Error %lu\n", GetLastError());
		return;
	}

	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event == 0)
		failure("Event creation error %lu\n", GetLastError());

	HANDLE thread = (HANDLE)_beginthreadex(nullptr, 0, Thread, nullptr, 0, nullptr);
	if (thread == 0)
	{
		printf("Thread start error %d\n", errno);
		return;
	}

	// Test that signals don't abort wait with EINTR error
	DWORD res = WaitForSingleObject(event, 500);
	if (!eventArrived)
	{
		failure("Expected console event hasn't arrived\n");
	}
	if (res != WAIT_TIMEOUT)
	{
		failure("Console event wait returned %lu instead of timeout\n", res);
	}
	CloseHandle(thread);
	CloseHandle(event);

	if (SetConsoleCtrlHandler(CtrlHandler, FALSE) == 0)
	{
		failure("Error unsetting user-side Ctrl-C handler. Error %lu\n", GetLastError());
		return;
	}
}
