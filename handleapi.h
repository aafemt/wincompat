/*********** Generic WinAPI handle ************/
#pragma once

#include <atomic>
#include "winnt.h"

// It is better to be a macro because some "other code" checks if it is defined
#define INVALID_HANDLE_VALUE (nullptr)

struct handle_t
{
    enum class handle_type { HANDLE_INVALID, HANDLE_FILE, HANDLE_EVENT, HANDLE_THREAD, HANDLE_TOKEN, HANDLE_FIND };
    handle_type type = handle_type::HANDLE_INVALID;

    virtual void release()
    {
        delete this;
    }

    virtual int GetWaitableHandle() = 0;
    // Is called by wait functions if this handle become signalled.
    // Returns true if wait succeeded indeed. If returned false, errno must be checked for error.
    virtual bool FinishWait() { return true; };

protected:
    virtual ~handle_t()
    {
        type = handle_type::HANDLE_INVALID; // Make use-after-delete a bit safer
    }
};

WINBOOL CloseHandle(volatile HANDLE& h);
