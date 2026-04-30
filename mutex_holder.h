#pragma once

#include <pthread.h>
// Instead of messing with pthread_cleanup_push/pop
struct MutexHolder
{
	pthread_mutex_t* m;
	MutexHolder(pthread_mutex_t* mutex) : m(mutex)
	{
		if (pthread_mutex_lock(m) != 0) // Quite impossible, but still...
		{
			m = nullptr;
		}
	}
	~MutexHolder()
	{
		if (m != nullptr)
		{
			pthread_mutex_unlock(m);
			m = nullptr;
		}
	}
};

#define MutexHolder(x) static_assert(false, "MutexHolder cannot be temporary")
