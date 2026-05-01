/************* Synchronization primitives ******************/
#pragma once

#include <pthread.h>
#include <stdlib.h>

#include "windef.h"

struct CRITICAL_SECTION
{
	pthread_mutex_t mutex;
	// Critical sections are often used in static objects.
	// But on Linux destruction order is even less predictable than on Windows.
	// Let's try to hide this problem.
	bool initialized = false;

	// Just to make sure that code comply to MS specs.
	CRITICAL_SECTION() = default;
	CRITICAL_SECTION(const CRITICAL_SECTION&) = delete;
	CRITICAL_SECTION(CRITICAL_SECTION&&) = delete;
	CRITICAL_SECTION& operator=(const CRITICAL_SECTION&) = delete;
};

inline void InitializeCriticalSection(CRITICAL_SECTION* lpCriticalSection)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lpCriticalSection->mutex, &attr);
	lpCriticalSection->initialized = true;
	pthread_mutexattr_destroy(&attr);
}

inline void DeleteCriticalSection(CRITICAL_SECTION* lpCriticalSection)
{
	lpCriticalSection->initialized = false;
	pthread_mutex_destroy(&lpCriticalSection->mutex);
}

inline void EnterCriticalSection(CRITICAL_SECTION* lpCriticalSection)
{
	if (lpCriticalSection->initialized)
		pthread_mutex_lock(&lpCriticalSection->mutex);
	// else to abort() or not to abort, that is the question
}

inline void LeaveCriticalSection(CRITICAL_SECTION* lpCriticalSection)
{
	// This function used to be used in destructors so it must not throw.
	// On the other hand an attempt to leave critical section that is not
	// owned is a serious bug so it is better to throw and cause std::terminate()
	// to be called for further debugging.
	if (lpCriticalSection->initialized && pthread_mutex_unlock(&lpCriticalSection->mutex) != 0)
	{
		abort();
	}
}
