/*******************************************************
*
*  Tests for synchronization functions
*
********************************************************/

#include <windows.h>
#include <process.h>

#include "globals.h"

volatile bool eventArrived = false;

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

volatile bool threadStarted = false;
volatile bool threadStopped = false;
unsigned WINAPI ShortThread(void*)
{
	threadStarted = true;
	return 1;
}

unsigned WINAPI LongerThread(void*)
{
	threadStarted = true;
	Sleep(100);
	threadStopped = true;
	return 2;
}

unsigned WINAPI LongThread(void*)
{
	threadStarted = true;
	Sleep(1000);
	threadStopped = true;
	return 3;
}

static volatile unsigned busyCounter = 0;
unsigned WINAPI BusyThread(void*)
{
	threadStarted = true;
	while (true || busyCounter < 1000000)
		++busyCounter;
	threadStopped = true;
	return 4;
}

void RunSyncTests()
{
	// Test waiting for threads

	// Test wait of already finished thread
	threadStarted = false;
	threadStopped = false;
	unsigned tid = 0;
	HANDLE thread = (HANDLE)_beginthreadex(nullptr, 0, ShortThread, nullptr, 0, &tid);
	if (thread == 0)
	{
		failure("Short thread start error %d\n", errno);
		return;
	}
	if (tid == 0)
	{
		failure("Thread is is zero\n");
	}

	while (!threadStarted)
	{
		Sleep(1);
	}
	Sleep(100); // Sleep some more to let thread really finish

	DWORD res = WaitForSingleObject(thread, 100);
	if (res != WAIT_OBJECT_0)
	{
		failure("Waiting for already stopped thread returned %lu instead of WAIT_OBJECT_0\n", res);
	}
	if (!GetExitCodeThread(thread, &res))
	{
		failure("GetExitCodeThread for already stopped thread returned error %lu\n", GetLastError());
	}
	if (res != 1)
	{
		failure("Already stopped thread exit code is %lu instead of 1\n", res);
	}
	if (!CloseHandle(thread))
	{
		failure("CloseHandle returned error %lu for already stopped thread\n", GetLastError());
		return;
	}

	// Test wait for not stopped yet thread
	threadStarted = false;
	threadStopped = false;

	thread = (HANDLE)_beginthreadex(nullptr, 0, LongerThread, nullptr, 0, nullptr);
	if (thread == 0)
	{
		failure("Longer thread start error %d\n", errno);
		return;
	}

	if (!GetExitCodeThread(thread, &res))
	{
		failure("GetExitCodeThread for running thread returned error %lu\n", GetLastError());
	}
	if (res != STILL_ACTIVE)
	{
		failure("Running thread exit code is %lu instead of STILL_ACTIVE\n", res);
	}

	res = WaitForSingleObject(thread, 0);
	if (res != WAIT_TIMEOUT)
	{
		failure("Short waiting for running thread returned %lu instead of WAIT_TIMEOUT\n", res);
	}

	res = WaitForSingleObject(thread, 300);
	if (res != WAIT_OBJECT_0)
	{
		failure("Longer waiting for running thread returned %lu instead of WAIT_OBJECT_0\n", res);
	}
	if (!GetExitCodeThread(thread, &res))
	{
		failure("GetExitCodeThread for longer thread returned error %lu\n", GetLastError());
	}
	if (res != 2)
	{
		failure("Longer thread exit code is %lx instead of 2\n", res);
	}
	if (!CloseHandle(thread))
	{
		failure("CloseHandle returned error %lu for longer thread\n", GetLastError());
		return;
	}

	// Test that detached thread finish work
	threadStarted = false;
	threadStopped = false;

	thread = (HANDLE)_beginthreadex(nullptr, 0, LongerThread, nullptr, 0, nullptr);
	if (thread == 0)
	{
		failure("Longer thread start error %d\n", errno);
		return;
	}
	if (!CloseHandle(thread))
	{
		failure("CloseHandle returned error %lu for detached longer thread\n", GetLastError());
	}
	for (int i = 0; i < 4; ++i)
	{
		if (!threadStopped)
		{
			Sleep(100);
		}
	}
	if (!threadStopped)
	{
		failure("Detached thread didn't stop\n");
	}

	// Test thread termination
	threadStarted = false;
	threadStopped = false;

	thread = (HANDLE)_beginthreadex(nullptr, 0, LongThread, nullptr, 0, nullptr);
	if (thread == 0)
	{
		failure("Long thread start error %d\n", errno);
		return;
	}
	while (!threadStarted)
	{
		Sleep(1);
	}
	if (!TerminateThread(thread, 666))
	{
		failure("TerminateThread returned error %lu\n", GetLastError());
	}
	res = WaitForSingleObject(thread, 200);
	if (res != WAIT_OBJECT_0)
	{
		failure("Waiting for terminated thread returned %lu instead of WAIT_OBJECT_0\n", res);
	}
	if (!GetExitCodeThread(thread, &res))
	{
		failure("GetExitCodeThread for terminated thread returned error %lu\n", GetLastError());
	}
	if (res != 666)
	{
		failure("Terminated thread exit code is %lu instead of 666\n", res);
	}
	if (!CloseHandle(thread))
	{
		failure("CloseHandle returned error %lu for terminated thread\n", GetLastError());
		return;
	}

	// Test that thread without cancellation points can be terminated
	threadStarted = false;
	threadStopped = false;

	thread = (HANDLE)_beginthreadex(nullptr, 0, BusyThread, nullptr, 0, nullptr);
	if (thread == 0)
	{
		failure("Busy thread start error %d\n", errno);
		return;
	}
	while (!threadStarted)
	{
		Sleep(1);
	}
	if (!TerminateThread(thread, 666))
	{
		failure("TerminateThread returned error %lu for busy thread\n", GetLastError());
	}
	res = WaitForSingleObject(thread, 200);
	if (res != WAIT_OBJECT_0)
	{
		failure("Waiting for terminated busy thread returned %lu instead of WAIT_OBJECT_0\n", res);
	}
	if (!GetExitCodeThread(thread, &res))
	{
		failure("GetExitCodeThread for terminated busy thread returned error %lu\n", GetLastError());
	}
	if (res != 666)
	{
		failure("Terminated busy thread exit code is %lu instead of 666\n", res);
	}
	if (!CloseHandle(thread))
	{
		failure("CloseHandle returned error %lu for busy terminated thread\n", GetLastError());
		return;
	}
	unsigned oldCounter = busyCounter;
	Sleep(10);
	if (oldCounter != busyCounter)
	{
		failure("Busy thread hasn't been terminated\n");
	}

	// Test that signals don't abort wait with EINTR error
	if (SetConsoleCtrlHandler(CtrlHandler, TRUE) == 0)
	{
		failure("Error setting user-side Ctrl-C handler. Error %lu\n", GetLastError());
		return;
	}

	HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (event == 0)
		failure("Event creation error %lu\n", GetLastError());

	thread = (HANDLE)_beginthreadex(nullptr, 0, Thread, nullptr, 0, nullptr);
	if (thread == 0)
	{
		printf("Thread start error %d\n", errno);
		return;
	}

	res = WaitForSingleObject(event, 500);
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
