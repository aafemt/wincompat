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
		if (res == WAIT_FAILED)
		{
			res = GetLastError();
			failure("Console event wait returned error %lx instead of timeout\n", res);
		}
		else
		{
			failure("Console event wait returned %lx instead of timeout\n", res);
		}
	}
	CloseHandle(thread);
	CloseHandle(event);

	if (SetConsoleCtrlHandler(CtrlHandler, FALSE) == 0)
	{
		failure("Error unsetting user-side Ctrl-C handler. Error %lu\n", GetLastError());
		return;
	}
	if (SetConsoleCtrlHandler(CtrlHandler, FALSE) != 0)
	{
		DWORD err = GetLastError();
		if (err != ERROR_INVALID_PARAMETER)
		{
			failure("Error unsetting non-user Ctrl-C handler. Error %lu instead of ERROR_INVALID_PARAMETER\n", err);
		}
	}
}
