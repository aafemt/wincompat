/********* Emulation of process and threads part of Win32 API *************/
#include <limits.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <unistd.h>
#include <atomic>

#include "windows.h"
#include "handleapi.h"
#include "process.h"
#include "mutex_holder.h"

struct ThreadHandle: public handle_t
{
    pthread_t handle;
    ThreadFunc* addr;
    void* args;
    volatile DWORD ExitCode = STILL_ACTIVE;
    // Handle that allows to wait for thread's finish.
    // pthread_t cannot be used for that because
    //  a) it is not fd and cannot be used in select/poll
    //  b) it become invalid after first join
    int waitHandle = -1;
    volatile long id = -1;
    pthread_mutex_t ExitCodeGuard = PTHREAD_MUTEX_INITIALIZER;

    // Simple reference counter is not enough for threads: there is a problem to choose moment for
    // detach of thread. It cannot be done on UsageCounter == 1 because
    // a) You cannot tell if other usage is thread itself or a duplicated handle;
    // b) while you are checking and going to detach thread someone else can drop counter to zero and free this handle.
    //
    // But inplementation with flags had different problem: the structure can be destructed while
    // parallel thread using it:
    // Thread 1 started thread 2 and wait for its finish.
    // Thread 3 performing cleanup and calls TerminateThread for thread 2.
    // (This is a bad idea in general but there is no other way to kill thread 2 while thread 1 is waiting.)
    // In exactly the same moment thread 2 is finished (either itself or by pthread_cancel) and get signalled
    // so thread 1 happily finish waiting and close the handle.
    // But due to CPU starvation thread 3 is still in TerminateThread and use the structure!!!
    //
    // So let's get back to reference counter and make every routine manipulating thread to keep own reference
    // including thread routine itself.
    // Fortunatelly pthread_cancel is implemented by abi::__forced_unwind exception and RAII holders work.
    std::atomic_int refCounter;

    struct Holder
    {
        ThreadHandle* h;
        Holder(ThreadHandle* thread) : h(thread)
        {
            // Safety first!
            if (h != nullptr && h->type == handle_type::HANDLE_THREAD)
                ++h->refCounter;
        }
        ~Holder()
        {
            // Yes, it is safe to delete nullptr, but modification of member after delete is hard to detect.
            if (h != nullptr && h->type == handle_type::HANDLE_THREAD)
            {
                if (--h->refCounter == 0)
                {
                    delete h;
                    h = nullptr;
                }
            }
        }
        ThreadHandle* operator->()
        {
            return h;
        }
    };

    // This structure owns itself so refCounter starts from 1.
    ThreadHandle(): refCounter(1)
    {
        type = handle_type::HANDLE_THREAD;
    };
    ~ThreadHandle() override;
    void release() override;
    int GetWaitableHandle() override;
};

ThreadHandle::~ThreadHandle()
{
    if (waitHandle != -1)
    {
        close(waitHandle);
        waitHandle = -1;
    }
}

void ThreadHandle::release()
{
    // This routine is called from CloseHandle.
    // Release self-ownership
    if (--refCounter == 0)
    {
        delete this;
    }
}

int ThreadHandle::GetWaitableHandle()
{
    // No critical section here because I don't believe that two threads can wait for this at the same time.
    if (waitHandle == -1)
    {
        // Following code is protected by mutex in case that thread is about finish right now
        MutexHolder guard(&ExitCodeGuard);

        int state = (ExitCode == STILL_ACTIVE) ? 0 : 1;
        waitHandle = eventfd(state, EFD_CLOEXEC | EFD_NONBLOCK);
    }
    return waitHandle;
}

WINBOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
    if (hThread == INVALID_HANDLE_VALUE || hThread->type != handle_t::handle_type::HANDLE_THREAD)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (lpExitCode == nullptr)
    {
        errno = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    ThreadHandle::Holder h(static_cast<ThreadHandle*>(hThread));

    // No waitting inside so keeping lock is safe here
    MutexHolder guarg(&h->ExitCodeGuard);
    if (h->ExitCode != STILL_ACTIVE)
    {
        *lpExitCode = h->ExitCode;
        return TRUE;
    }

    // ExitCode STILL_ALIVE doesn't mean that the thread is really alive.
    // Let's check it.
    void* result;
    switch ((errno = pthread_tryjoin_np(h->handle, &result)))
    {
    case EBUSY:
        *lpExitCode = STILL_ACTIVE;
        return TRUE;

    case 0:
        // Join is a destructive operation, any attempt to join or detach this thread afterward
        // is UB, so we HAVE TO set exit code to something other than STILL_ALIVE to prevent
        // joining from any other place.

        if (h->ExitCode == STILL_ACTIVE) // That's a lie!!!
        {
                DWORD NewExitCode = (uintptr_t)(result) & 0xFFFFFFFF;
                if (NewExitCode == STILL_ACTIVE) // Still possible on 32 bits.
                {
                    NewExitCode = 0xBAD;
                }
                h->ExitCode = NewExitCode;
        }
        // else the thread managed to set exit code and we must not change it.
        *lpExitCode = h->ExitCode;

        // Just in case that thread failed to signal its finish
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }

        return TRUE;
    }
    return FALSE;
}

WINBOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
    if (hThread == INVALID_HANDLE_VALUE || hThread->type != handle_t::handle_type::HANDLE_THREAD)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (dwExitCode == STILL_ACTIVE)
    {
        // Bad exit code, replace it with something else
        dwExitCode = 0xBAD;
    }

    ThreadHandle::Holder h(static_cast<ThreadHandle*>(hThread));
    if (h->ExitCode != STILL_ACTIVE) // This thread is already finished or detached.
    {
        // Consider it as a success
        return TRUE;
    }
    if ((errno = pthread_cancel(h->handle)) == 0)
    {
        // Cancellation signal has been sent but even asynchronous cancellation doesn't guarantee immediate stop.
        bool mustWait = false;
        DWORD oldExitCode = STILL_ACTIVE;
        { // Scope
            // We cannot wait inside of lock and if we leave ExitCode STILL_ALIVE, someone can join inbetween.
            MutexHolder guard(&h->ExitCodeGuard);
            oldExitCode = h->ExitCode;
            if (h->ExitCode == STILL_ACTIVE)
            {
                h->ExitCode = dwExitCode;
                mustWait = true;
            }
        }
        if (mustWait)
        {
            void* result = PTHREAD_CANCELED;
            timespec t{time(nullptr) + 1, 0};
            if ((errno = pthread_timedjoin_np(h->handle, &result, &t)) != 0)
            {
                return FALSE;
            }
            // If thread wasn't really cancelled and returned a valid exit code
            if (result != PTHREAD_CANCELED)
            {
                h->ExitCode = oldExitCode;
            }
        }
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }
        return TRUE;
    }
    if (errno == ESRCH) // No such thread, consider it as a success as well
    {
        // Also success but the thread died first.
        MutexHolder guard(&h->ExitCodeGuard);
        if (h->ExitCode == STILL_ACTIVE)
        {
            h->ExitCode = dwExitCode;
        }
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }
    }
    return FALSE;
}

// this function is an adapter between different thread function declarations in Windows and POSIX
static void* thread_impl(void* param)
{
    ThreadHandle::Holder h(static_cast <ThreadHandle*>(param));

    try
    {
        // Do it ASAP to let _beginthreadex return right tid
        h->id = GetCurrentThreadId();

        // Allow TerminateThread to be dangerous and kill even busy threads without cancellation points inside.
        int oldType;
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType);

        // Execute user code
        unsigned result = h->addr(h->args);

        MutexHolder guard(&(h->ExitCodeGuard));

        h->ExitCode = result;
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }
        // That's all, folks
        return reinterpret_cast<void*>(result);
    }
    catch (...) // Most likely it is abi::__forced_unwind. Or a but in user code that let exception fly away
    {
        MutexHolder guard(&(h->ExitCodeGuard));
        if (h->ExitCode == STILL_ACTIVE)
        {
            h->ExitCode = (DWORD)PTHREAD_CANCELED;
        }
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }
        throw;
    }
}

HANDLE _beginthreadex(void*, unsigned stack_size, ThreadFunc* start_address, void* arglist, unsigned, unsigned* tid)
{
    if (start_address == nullptr)
    {
        errno = EINVAL;
        return 0;
    }
    ThreadHandle* result = new ThreadHandle;
    result->addr = start_address;
    result->args = arglist;
    pthread_attr_t attr;
    int attr_initialized = -1;
    if (stack_size != 0 && stack_size >= (unsigned)(PTHREAD_STACK_MIN))
    {
        attr_initialized = pthread_attr_init(&attr);
        if (attr_initialized == 0)
            pthread_attr_setstacksize(&attr, stack_size);
    }
    // TODO: support initialization flags as well.
    if ((errno = pthread_create(&(result->handle), attr_initialized == 0? &attr: nullptr, thread_impl, result)) != 0)
    {
        if (attr_initialized == 0)
        {
            pthread_attr_destroy(&attr);
        }
        delete result;
        return 0;
    }
    if (attr_initialized == 0)
    {
        pthread_attr_destroy(&attr);
    }
    if (tid != nullptr)
    {
        int attempts = 0;
        while (result->id == -1 && ++attempts < 100) // I hope 100 is enough
        {
            // Despite recommendations, sleep(0) is the best way for busy wait.
            // sched_yield() would require to raise number attempts to several thousands.
            // Following test programs shows 1-2 for sleep(0), 20-2000 for sched_yeld()
            // and 1000-10000 for nothing.
            //volatile int i = 0;
            //
            //void* thread_func(void*)
            //{
            //	i = 1;
            //	return nullptr;
            //}
            //
            //
            //int main()
            //{
            //	while (true)
            //	{
            //		int n = 0;
            //		i = 0;
            //		pthread_t thread;
            //		pthread_create(&thread, nullptr, thread_func, nullptr);
            //		while (i == 0)
            //		{
            //			//sleep(0);
            //          //sched_yield();
            //			++n;
            //		}
            //		printf("Thread started %d\n", n);
            //		if (int err = pthread_join(thread, nullptr))
            //		{
            //			printf("join error %d\n", err);
            //		}
            //		sleep(1);
            //	}
            //}

            // On the other hand condition variable could be used here, perhaps...
            sleep(0);
        }
        *tid = result->id;
    }
    return result;
}

DWORD GetCurrentProcessId()
{
    return getpid();
}

DWORD GetCurrentThreadId()
{
	return syscall(SYS_gettid);
}
