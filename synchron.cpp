/************ Synchronization functions implementation **************/

#include <memory>
#include <poll.h>
#include <time.h>

#include "windows.h"

static bool HandleIsWaitable(HANDLE Handle)
{
    if (Handle == INVALID_HANDLE_VALUE)
    {
        errno = ERROR_INVALID_HANDLE;
        return false;
    }
    bool r = Handle->type == handle_t::handle_type::HANDLE_EVENT ||
             Handle->type == handle_t::handle_type::HANDLE_THREAD ||
             Handle->type == handle_t::handle_type::HANDLE_FILE;
    if (!r)
    {
        errno = ERROR_ACCESS_DENIED; // Dunno why but Windows returns that
    }
    return r;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    if (!HandleIsWaitable(hHandle))
    {
        return WAIT_FAILED;
    }

    timespec b;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &b) != 0)
    {
        return WAIT_FAILED;
    }

    while (true)
    {
        pollfd fd = { hHandle->GetWaitableHandle(), POLLIN, 0 };
        if (fd.fd == -1)
        {
            // errno = ERROR_INVALID_HANDLE; Do not set it, preserve original error code
            return WAIT_FAILED;
        }
        switch (poll(&fd, 1, dwMilliseconds))
        {
        case -1:
            // WaitFor* is not interrupted by signals
            if (errno == EINTR)
            {
                // Still need to adjust timeout
                break;
            }
            return WAIT_FAILED;
        case 0:
            return WAIT_TIMEOUT;
        default:
            errno = 0;
            if (hHandle->FinishWait())
                return WAIT_OBJECT_0;
            if (errno != 0)
            {
                return WAIT_FAILED;
            }
        }
        timespec e;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &e) == 0)
        {
            // Adjust timeout for number of passed milliseconds
            dwMilliseconds -= (e.tv_sec - b.tv_sec) * 1000;
            dwMilliseconds -= (e.tv_nsec - b.tv_nsec) / 1000000;
            b = e;
        }
        else
        {
            return WAIT_FAILED;
        }
    }
}

DWORD WaitForMultipleObjects(DWORD nCount, HANDLE* lpHandles, WINBOOL bWaitAll, DWORD dwMilliseconds)
{
    for (DWORD i = 0; i < nCount; i++)
    {
        if (!HandleIsWaitable(lpHandles[i]))
        {
            return WAIT_FAILED;
        }
    }

    // Fill array of objects
    std::unique_ptr<pollfd[]> fds(new pollfd[nCount]);
    for (DWORD i = 0; i < nCount; i++)
    {
        pollfd& fd = fds[i];
        fd.fd = lpHandles[i]->GetWaitableHandle();
        if (fd.fd == -1)
            return WAIT_FAILED;
        fd.events = POLLIN;
        fd.revents = 0;
    }

    timespec b;
    clock_gettime(CLOCK_MONOTONIC_RAW, &b);
    while (true)
    {
        switch (poll(fds.get(), nCount, dwMilliseconds))
        {
        case -1:
            //printf("poll wait failed\n");
            if (errno == EINTR)
            {
                // Still need to adjust timeout
                break;
            }
            return WAIT_FAILED;
        case 0:
            //printf("poll wait timeouted\n");
            return WAIT_TIMEOUT;
        default:
            //printf("poll returned %d\n", res);
            // Scan fds to find out if wait condition is satisfied
            errno = 0;
            if (bWaitAll == TRUE)
            {
                bool finished = true;
                for (DWORD i = 0; i < nCount; i++)
                {
                    pollfd& fd = fds[i];
                    if (fd.fd >= 0)
                    {
                        if (fd.revents != 0)
                        {
                            // Ok, mark this handle as signalled
                            fd.fd = -fd.fd;
                        }
                        else
                            finished = false;
                    }
                }
                if (finished)
                {
                    // This piece of code has behavior different from MS implementation.
                    // Good implementation would require atomic reset of signalled state for all handles
                    // which is hard or impossible here.
                    for (DWORD i = 0; i < nCount; i++)
                    {
                        errno = 0;
                        pollfd& fd = fds[i];
                        if (!lpHandles[i]->FinishWait())
                        {
                            if (errno != 0)
                                return WAIT_FAILED;
                            // No error, signalled state was reset by someone else
                            // Continue wait for this handle
                            fd.fd = -fd.fd;
                            finished = false;
                        }
                    }
                    if (finished)
                    {
                        return WAIT_OBJECT_0;
                    }
                }
            }
            else
            {
                for (DWORD i = 0; i < nCount; i++)
                {
                    if (fds[i].fd >= 0 && fds[i].revents != 0 && lpHandles[i]->FinishWait())
                        return WAIT_OBJECT_0 + i;

                    if (errno != 0) // set in FinishWait()
                    {
                        return WAIT_FAILED;
                    }
                }
            }
        }
        timespec e;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &e) == 0)
        {
            // Adjust timeout for number of passed milliseconds
            DWORD timePassed = (e.tv_sec - b.tv_sec) * 1000 + (e.tv_nsec - b.tv_nsec) / 1000000;
            if (timePassed >= dwMilliseconds)
            {
                return WAIT_TIMEOUT;
            }
            dwMilliseconds -= timePassed;
            b = e;
        }
        else
        {
            return WAIT_FAILED;
        }
    }
}

#include <sched.h>

void Sleep(int Milliseconds)
{
    // It is not quite perfect emulation because even with INFINITE interval
    // it will return on signal, but good enough for our purposes.
    if (Milliseconds == 0)
        sched_yield();
    else
        poll(nullptr, 0, Milliseconds);
}
