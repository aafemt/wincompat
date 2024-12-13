/********* Emulation of process and threads part of Win32 API *************/
#include <limits.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "windows.h"
#include "handleapi.h"
#include "process.h"

struct ThreadHandle: public handle_t
{
    pthread_t handle;
    ThreadFunc* addr;
    void* args;
    DWORD ExitCode = STILL_ACTIVE;
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
    struct is
    {
        static const int detached = 1;
        static const int finished = 2;
    };
    std::atomic_int flags;

    ThreadHandle(): flags(0)
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
    if ((flags & is::detached) == 0)
    {
        // This call can fail if thread already finished, so ignore returning value
        pthread_detach(handle);
    }
    // Setting of "detached" flag if it is already set is safe and we _must_ perform it in any case.
    if ((flags.fetch_or(is::detached) & is::finished) != 0)
    {
        delete this;
    }
    // No code below this point because this handle can be being deleted by thread routine.
}

int ThreadHandle::GetWaitableHandle()
{
    // No critical section here because I don't believe that two threads can wait for this at the same time.
    if (waitHandle == -1)
    {
        // Following code is protected by mutex in case that thread is about finish right now
        pthread_mutex_lock(&ExitCodeGuard);
        int state = (ExitCode == STILL_ACTIVE)? 0 : 1;
        // Protected section includes mutex creation for simplification
        waitHandle = eventfd(state, EFD_CLOEXEC | EFD_NONBLOCK);
        pthread_mutex_unlock(&ExitCodeGuard);
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
    ThreadHandle* h = static_cast<ThreadHandle*>(hThread);
    if ((h->flags & ThreadHandle::is::finished) != 0)
    {
        // Bug protection.
        if (h->ExitCode == STILL_ACTIVE) // Bad value, may cause endless loop by MSDN version.
        {
            h->ExitCode = 0xBAD;
        }
        *lpExitCode = h->ExitCode;
        return TRUE;
    }
    // Even if out code always set "finished" flag on thread end or termination,
    // thread can be killed by someone else, so we must check if it really alive.
    void* result;
    switch ((errno = pthread_tryjoin_np(h->handle, &result)))
    {
    case EBUSY:
        // ExitCode must not be assigned here because of thread race
        *lpExitCode = STILL_ACTIVE;
        return TRUE;
    case 0:
        // At this point we are sure that thread is dead and won't interfere us, so we can work w/o synchronization.
        // Join is a destructive operation, any attempt to join or detach this thread afterward
        // is UB, mark it as both finished and detached, but do not release handle till CloseHandle().
        h->flags |= ThreadHandle::is::finished | ThreadHandle::is::detached;
        if (h->ExitCode == STILL_ACTIVE)
        {
                DWORD NewExitCode = (uintptr_t)(result) & 0xFFFFFFFF;
                if (NewExitCode == STILL_ACTIVE)
                {
                    NewExitCode = 0xBAD;
                }
                // If this code is executed in parallel with TerminateThread(), exit code may get
                // value PTHREAD_CANCELLED instead of what it is supposed to.
                // Let's ignore this case.
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
    ThreadHandle* h = static_cast<ThreadHandle*>(hThread);
    if (h->flags != 0) // This thread is already finished or detached. We cannot cancel detached thread
    {
        // Consider it as a success
        h->ExitCode = dwExitCode;
        return TRUE;
    }
    if ((errno = pthread_cancel(h->handle)) == 0)
    {
        // Cancellation signal has been sent but thread may still heading to cancellation point.
        // It makes a problem: if we mark thread as finished and return following CloseHandle() will
        // destroy this handle, but thread may be still active and by chance it is in finalization
        // stage, accessing struct members.
        // If we don't mark it as finished, CloseHandle() won't destroy handle but thread will never
        // reach finalization code, so we'll have memory leak.
        // To solve that join it right here.
        // It may hang in which case timeouted join or asynchronous cancel can be used
        // but so far let's trust our own threads.
        void* result;
        // Result of this call is irrelevant, most of errors means that the thread is already dead
        // (except, may be EINTR)
        if (pthread_join(h->handle, &result) == 0)
        {
            // Join succeeded, prevent it from repeating
            h->flags |= ThreadHandle::is::detached;
        }
        // Flag the thread as finished because it hardly can do it itself now.
        h->flags |= ThreadHandle::is::finished;
        // Don't use ExitCodeGuard here because it may be held by killed thread which will cause deadlock.
        h->ExitCode = dwExitCode;
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }
        return TRUE;
    }
    if (errno == ESRCH) // No such thread, consider it as a success as well
    {
        h->flags |= ThreadHandle::is::finished | ThreadHandle::is::detached;
        h->ExitCode = dwExitCode;
        if (h->waitHandle != -1)
        {
            eventfd_write(h->waitHandle, 1);
        }
        return TRUE;
    }
    return FALSE;
}

// this function is an adapter between different thread function declarations in Windows and POSIX
static void* thread_impl(void* param)
{
    ThreadHandle* h = static_cast <ThreadHandle*>(param);
    h->id = GetCurrentThreadId();
    // Execute user code
    unsigned result = h->addr(h->args);
    // Following piece must be synchronized with GetWaitableHandle()
    pthread_mutex_lock(&(h->ExitCodeGuard));
    h->ExitCode = result;
    if (h->waitHandle != -1)
    {
        eventfd_write(h->waitHandle, 1);
    }
    pthread_mutex_unlock(&(h->ExitCodeGuard));
    // That's all, folks
    if ((h->flags.fetch_or(ThreadHandle::is::finished) & ThreadHandle::is::detached) != 0)
    {
        delete h;
    }
    // No code here is allowed to access h even if it wasn't deleted by line above.
    return reinterpret_cast<void*>(result);
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
