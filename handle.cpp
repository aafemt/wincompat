/*********** Emulation of WinAPI handle ***********/
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <poll.h>

#include "windows.h"

WINBOOL CloseHandle(HANDLE& h)
{
    if (reinterpret_cast<intptr_t>(h) < 0 && reinterpret_cast<intptr_t>(h) > -100)
    {
        // Pseudohandles, ignore them
        return TRUE;
    }
    if (h == INVALID_HANDLE_VALUE || h->type == handle_t::handle_type::HANDLE_INVALID)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    h->release();
    // It is pity that we cannot invalidate handle. Or may be we can?.. Let's try.
    h = INVALID_HANDLE_VALUE;
    return TRUE;
}

struct EventHandle: public handle_t
{
    int handle;
    bool ManualReset;
public:
    EventHandle(WINBOOL bManualReset, int h): handle(h), ManualReset(bManualReset == TRUE)
    {
        type = handle_type::HANDLE_EVENT;
    }
    ~EventHandle();
    int GetWaitableHandle() override { return handle; }
    bool FinishWait() override;
};

EventHandle::~EventHandle()
{
    if (handle != -1)
        close(handle);
}

bool EventHandle::FinishWait()
{
    if (!ManualReset)
    {
        eventfd_t value;
        if (eventfd_read(handle, &value) == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // This is not really an error, someone has reset this event before us, continue wait.
                errno = 0;
            }
            return false;
        }
    }
    return true;
}

HANDLE CreateEvent(void*, WINBOOL bManualReset, WINBOOL bInitialState, char*)
{
    int h = eventfd(bInitialState, EFD_CLOEXEC | EFD_NONBLOCK);
    if (h == -1)
        return INVALID_HANDLE_VALUE;
    else
        return new EventHandle(bManualReset, h);
}

WINBOOL SetEvent(HANDLE event)
{
    if (event == INVALID_HANDLE_VALUE || event->type != event->handle_type::HANDLE_EVENT)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (eventfd_write(static_cast<EventHandle*>(event)->handle, 1) == -1)
        return FALSE;
    else
        return TRUE;
}

WINBOOL ResetEvent(HANDLE event)
{
    if (event == INVALID_HANDLE_VALUE || event->type != event->handle_type::HANDLE_EVENT)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    int h = static_cast<EventHandle*>(event)->handle;

    pollfd fd = { h, POLLIN, 0 };
    switch (poll(&fd, 1, 0))
    {
    case 1: // Event is signalled
        eventfd_t value;
        if (eventfd_read(h, &value) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            return FALSE;
        __attribute__((fallthrough));
    case 0: // event is already non-signalled
        return TRUE;
    default: // -1 means error
        return FALSE;
    }
}
