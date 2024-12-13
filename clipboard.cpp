/******************************************************

  Windows Clipboard API emulation

*******************************************************/

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <xcb/xcb.h>

#include "winuser.h"

namespace
{
    pthread_mutex_t x_mutex = PTHREAD_MUTEX_INITIALIZER;
    // Event to signal clipboard operation completion
    int CompletionEvent = -1;
    bool MustWait = false;
    xcb_window_t window;
    xcb_atom_t XA_CLIPBOARD;
    xcb_atom_t XA_TARGETS;
    xcb_atom_t XA_UTF8_STRING;


    // Windows GUI-related errors seems to be in range 2000-3000.
    // Actually, it doesn't matter.
    const int ERROR_GUI_BASE = 2000;

    struct ClipboardContent
    {
        ClipboardContent* next = nullptr;
        unsigned ContentType = 0;
        HGLOBAL data = nullptr;
    };
    ClipboardContent* content = nullptr;

    void* EventHandlingThread(void* param)
    {
        xcb_connection_t* conn = reinterpret_cast<xcb_connection_t*>(param);
        xcb_generic_event_t* event;
        while ((event = xcb_wait_for_event(conn)) != nullptr)
        {
            switch(event->response_type)
            {
            case 0:
                // Any kind of error
                return nullptr;
            case XCB_SELECTION_CLEAR:
                {
                    // For time being we clear global content. May be at some point a better management
                    // should be implemented: content tied to owner window.
                    if (pthread_mutex_lock(&x_mutex) == 0)
                    {
                        while (content != nullptr)
                        {
                            ClipboardContent* tmp = content;
                            content = content->next;
                            free(tmp->data);
                            delete tmp;
                        }
                        // Signal completion to let CloseClipboard() return faster.
                        // If it already timed out - nevermind.
                        pthread_mutex_unlock(&x_mutex);
                        eventfd_write(CompletionEvent, 1);
                    }
                    break;
                }
            case XCB_SELECTION_REQUEST:
                {
                    xcb_selection_request_event_t* e = (xcb_selection_request_event_t*)event;

                    xcb_selection_notify_event_t answer;
                    memset(&answer, 0, sizeof(answer));
                    answer.response_type = XCB_SELECTION_NOTIFY;
                    answer.requestor = e->requestor;
                    answer.selection = e->selection;
                    answer.target = e->target;
                    answer.property = XCB_NONE;
                    if (e->target != XCB_NONE)
                    {
                        if (e->target == XA_TARGETS)
                        {
                            xcb_atom_t targets[10] = { XA_TARGETS };
                            // lock mutex before accessing content list
                            // May be it can be done a bit earlier?..
                            if (pthread_mutex_lock(&x_mutex) == 0)
                            {
                                int n = 1;
                                for (ClipboardContent* itr = content; itr != nullptr; itr = itr->next)
                                {
                                    targets[n++] = itr->ContentType;
                                    if (n >= 10)
                                        break;
                                }
                                xcb_change_property(conn, XCB_PROP_MODE_REPLACE, e->requestor, e->property, XCB_ATOM_ATOM, 32, n, targets);
                                answer.property = e->property;
                                pthread_mutex_unlock(&x_mutex);
                                // Do not set CompletionEvent here because it is not the end yet
                            }
                        }
                        else
                        {
                            if (pthread_mutex_lock(&x_mutex) == 0)
                            {
                                GlobalMemory* data = nullptr;
                                for (ClipboardContent* itr = content; itr != nullptr; itr = itr->next)
                                {
                                    if (itr->ContentType == e->target)
                                    {
                                        data = itr->data;
                                        break;
                                    }
                                }
                                if (data == nullptr) // Second chance for stupid requestor asking for something we don't have
                                {
                                    for (ClipboardContent* itr = content; itr != nullptr; itr = itr->next)
                                    {
                                        if ((e->target == XA_UTF8_STRING && itr->ContentType == XCB_ATOM_STRING) ||
                                            (e->target == XCB_ATOM_STRING && itr->ContentType == XA_UTF8_STRING))
                                        {
                                            data = itr->data;
                                            break;
                                        }
                                    }
                                }
                                if (data != nullptr)
                                {
                                    uint32_t DataLength = data->AllocatedSize;
                                    if (e->target == XCB_ATOM_STRING || e->target == XA_UTF8_STRING)
                                    {
                                        // In contrast to Window here terminating '\0' is not expected to be a part of data.
                                        DataLength = strlen(data->data);
                                    }
                                    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, e->requestor, e->property, e->target, 8, DataLength, data->data);
                                    answer.property = e->property;
                                }
                                pthread_mutex_unlock(&x_mutex);
                            }
                        }
                    }
                    xcb_send_event(conn, 0, e->requestor, 0, (const char*)(&answer));
                    xcb_flush(conn);
                    break;
                }
            default:
                break;
            }
            free(event);
        }
        return nullptr;
    }

    // Global connection declared after thread routine to prevent copy-paste errors.
    xcb_connection_t* x = nullptr;
}

WINBOOL OpenClipboard(HWND)
{
    int err = 0;
    MustWait = false;
    // Lock mutex
    if (pthread_mutex_lock(&x_mutex) != 0)
        return FALSE;

    if (x != nullptr)
    {
        // Check if connection is still valid
        if (xcb_connection_has_error(x) != 0)
        {
            // Dats bad
            xcb_disconnect(x);
            x = nullptr;
        }
    }

    // Initialize X connection
    if (x == nullptr)
    {
        const char* DISPLAY = getenv("DISPLAY");
        if (DISPLAY == nullptr)
        {
            DISPLAY = ":0";
        }
        // If this connection ever is used for GUI too - find out and set preferred screen.
        x = xcb_connect(DISPLAY, nullptr);
        err = xcb_connection_has_error(x);
        if (err != 0)
        {
            err += ERROR_GUI_BASE;
            xcb_disconnect(x);
            x = nullptr;
        }
        else
        {
            if (CompletionEvent == -1)
            {
                CompletionEvent = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
            }
            // Create a window to be clipboard owner
            window = xcb_generate_id(x);
            xcb_create_window(x, XCB_COPY_FROM_PARENT, window,
                              xcb_setup_roots_iterator(xcb_get_setup(x)).data->root,
                              1, 1, 100, 100, 0,
                              XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
            // Get essential atoms
            xcb_intern_atom_cookie_t clip_cookie = xcb_intern_atom(x, false, 9, "CLIPBOARD");
            xcb_intern_atom_cookie_t targets_cookie = xcb_intern_atom(x, false, 7, "TARGETS");
            xcb_intern_atom_cookie_t utf_string_cookie = xcb_intern_atom(x, false, 11, "UTF8_STRING");
            xcb_generic_error_t* err_reply;
            xcb_intern_atom_reply_t* atom_reply = xcb_intern_atom_reply(x, clip_cookie, &err_reply);
            if (atom_reply != nullptr)
            {
                XA_CLIPBOARD = atom_reply->atom;
                free(atom_reply);
                atom_reply = xcb_intern_atom_reply(x, targets_cookie, &err_reply);
                if (atom_reply != nullptr)
                {
                    XA_TARGETS = atom_reply->atom;
                    free(atom_reply);
                    atom_reply = xcb_intern_atom_reply(x, utf_string_cookie, &err_reply);
                    if (atom_reply != nullptr)
                    {
                        XA_UTF8_STRING = atom_reply->atom;
                        free(atom_reply);
                    }
                    else
                    {
                        err = err_reply->error_code;
                        free(err_reply);
                    }
                }
                else
                {
                    err = err_reply->error_code;
                    free(err_reply);
                }
            }
            else
            {
                err = err_reply->error_code;
                free(err_reply);
            }
        }
        if (err == 0)
        {
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, 1);
            pthread_t dummy;
            pthread_create(&dummy, &attr, EventHandlingThread, x);
            pthread_attr_destroy(&attr);
        }
    }


    if (err != 0)
    {
        errno = err;
        // Unlock mutex now because CloseConsole() won't be called.
        pthread_mutex_unlock(&x_mutex);
        return FALSE;
    }
    return TRUE;
}

WINBOOL EmptyClipboard()
{
    while (content != nullptr)
    {
        ClipboardContent* tmp = content;
        content = content->next;
        free(tmp->data);
        delete tmp;
    }

    // Reset event before acquiring ownership to let CloseClipboard() wait for reaction from clipboard manager
    eventfd_t dummy;
    // Here we are not interested in event state thus ignore any error except EBADF
    if (read(CompletionEvent, &dummy, sizeof(dummy)) == -1 && errno == EBADF)
    {
        // reinitialize handle;
        CompletionEvent = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    }

    xcb_get_selection_owner_cookie_t own_cookie = xcb_get_selection_owner(x, XA_CLIPBOARD);
    xcb_get_selection_owner_reply_t* owner_reply = xcb_get_selection_owner_reply(x, own_cookie, nullptr);
    // No point to re-gain ownership
    if (owner_reply == nullptr || owner_reply->owner != window)
    {
        xcb_set_selection_owner(x, window, XA_CLIPBOARD, XCB_CURRENT_TIME);
        xcb_flush(x);
    }
    return TRUE;
}

void SetClipboardData(unsigned uFormat, HGLOBAL data)
{
    if (data == nullptr)
    {
        // TODO: handle owner-draw clipboard
        return;
    }

    uint32_t newType = uFormat;
    switch (uFormat)
    {
    case CF_TEXT: newType = XCB_ATOM_STRING; break;
    /*
    case CF_UNICODETEXT: newType = XA_UTF8_STRING;
    TODO: Conversion from unicode to UTF-8
    */
    }

    ClipboardContent* prev = nullptr;
    for (ClipboardContent* itr = content; itr != nullptr; itr = itr->next)
    {
        prev = itr;
        if (itr->ContentType == newType)
        {
            break;
        }
    }
    if (prev != nullptr)
    {
        if (prev->ContentType == newType)
        {
            free(prev->data);
            prev->data = data;
            return;
        }
        prev->next = new ClipboardContent;
        prev = prev->next;
    }
    else
    {
        content = new ClipboardContent;
        prev = content;
    }
    prev->ContentType = newType;
    prev->data = data;

    // After content of clipboard changed, CloseClipboard() must not return immediatelly
    // giving recipient(s) some time to retrieve new content.
    MustWait = true;
}

WINBOOL CloseClipboard()
{
    if ((errno = pthread_mutex_unlock(&x_mutex)) != 0)
        return FALSE;

    if (MustWait)
    {
        // On one hand we can be in a console application that is finished immediatelly after SetClipboardData()
        // In this case we must wait a little to let recipient (clipboard manager) to retrieve content.
        // On the other hand if application continue to work, we must not cause a noticeable delay.
        pollfd fd = { CompletionEvent, POLLIN, 0};
        if (poll(&fd, 1, 100) > 0)
        {
            eventfd_t dummy;
            eventfd_read(CompletionEvent, &dummy);
        }
        // errors and timeouts may be safetly ignored here
    }
//sleep(5);
    return TRUE;
}
