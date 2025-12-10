#include <stdlib.h>

#include "wincon.h"
#include "winerror.h"

namespace
{
	struct Handler
	{
		Handler* next;
		PHANDLER_ROUTINE routine;
	};

	Handler* handlers = nullptr;
	// Initialize saved action to default action for simplification
	struct sigaction savedInt = {};
	struct sigaction savedQuit = {};
	struct sigaction savedHup = {};
	struct sigaction savedTerm = {};
	static_assert(SIG_DFL == nullptr, "Zero-initialization cannot work");
	bool CtrlCDisabled = false;

	void handleThat(int signal, siginfo_t* info, void* param)
	{
		// This setting is above all
		if (signal == SIGINT && CtrlCDisabled)
		{
			return;
		}
		for (Handler* itr = handlers; itr != nullptr; itr = itr->next)
		{
			if (itr->routine(signal) == TRUE)
			{
				// Signal handled
				return;
			}
		}
		// Signal not handled. Invoke default action.
		// It is recommended to restore old action here and raise
		// but it has a little problem of delivering different signal info
		// into previously installed handler
		struct sigaction* action = nullptr;
		enum class Action
		{
			Term,
			Ign,
			Core,
			//Stop,
			//Cont
		};
		Action def;
		switch (signal)
		{
		case SIGINT:
			{
				action = &savedInt;
				def = Action::Term;
				break;
			}
		case SIGQUIT:
			{
				action = &savedQuit;
				def = Action::Core;
				break;
			}
		case SIGHUP:
			{
				action = &savedHup;
				def = Action::Term;
				break;
			}
		case SIGTERM:
			{
				action = &savedTerm;
				def = Action::Term;
				break;
			}
		default:
			// We do not handle any other signal, so this code should never be executed
			// but in case if impossible happen - create core dump for debugging.
			def = Action::Core;
			break;
		}
		if (action != nullptr)
		{
			if (action->sa_flags & SA_SIGINFO)
			{
				action->sa_sigaction(signal, info, param);
				return;
			}
			else if (action->sa_handler == SIG_IGN)
			{
				def = Action::Ign;
			}
			else if (action->sa_handler != SIG_DFL)
			{
				action->sa_handler(signal);
				return;
			}
		}
		// No saved action or the action is default
		switch (def)
		{
		case Action::Term:
			exit(EXIT_FAILURE);
		case Action::Ign:
			return;
		case Action::Core:
			abort();
		}
	}
}

// this routine is not thread safe. Should it be?..
BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add)
{
	if (HandlerRoutine == nullptr)
	{
		// Disabling of Ctrl-C and setting of a handler are completely separated,
		// so setting of a handler should not enable signal delivery
		CtrlCDisabled = Add == TRUE;
		if (handlers == nullptr)
		{
			// Our handler is not set, enable/disable processing directly
			struct sigaction s{};
			struct sigaction old;
			if (Add == TRUE)
			{
				// If new action is ignore, simply set it to ignore.
				s.sa_handler = SIG_IGN;
				sigfillset(&s.sa_mask);
				s.sa_flags = 0;
			}
			else
			{
				// Otherwise restore old action (which may be default or user-defined)
				// This branch is not recursive, so two calls with TRUE and one with FALSE ends up in FALSE.
				s = savedInt;
			}
			if (sigaction(SIGINT, &s, &old) != 0)
			{
				return FALSE;
			}
			// If signal was already ignored - do not override the saved (user or default) handler
			// If signal was user-handled (not our) - preserve it for further restoring
			if (Add == TRUE && old.sa_handler != SIG_IGN && old.sa_handler != SIG_DFL)
			{
				savedInt = old;
			}
		}
	}
	else
	{
		if (Add == TRUE)
		{
			// MSDN:
			// When a console process receives any of the control signals, its handler
			// functions are called on least-registered, first-called basis.
			Handler* newHandler = new Handler;
			newHandler->next = handlers;
			newHandler->routine = HandlerRoutine;
			handlers = newHandler;

			if (newHandler->next == nullptr)
			{
				// Install the real handlers
				struct sigaction s = {};
				struct sigaction old;
				s.sa_sigaction = handleThat;
				sigfillset(&s.sa_mask);
				s.sa_flags = SA_SIGINFO;
				if (sigaction(CTRL_C_EVENT, &s, &old) != 0)
				{
					return FALSE;
				}
				// It is strange but possible that application already set the handler by POSIX means
				if (old.sa_handler != SIG_DFL && old.sa_handler != SIG_IGN && old.sa_sigaction != handleThat)
				{
					savedInt = old;
				}
				// Do the same for other potentially handled signals
				if (sigaction(CTRL_BREAK_EVENT, &s, &old) != 0)
				{
					return FALSE;
				}
				if (old.sa_handler != SIG_DFL && old.sa_sigaction != handleThat)
				{
					savedQuit = old;
				}
				if (sigaction(CTRL_CLOSE_EVENT, &s, &old) != 0)
				{
					return FALSE;
				}
				if (old.sa_handler != SIG_DFL && old.sa_sigaction != handleThat)
				{
					savedHup = old;
				}
				if (sigaction(CTRL_SHUTDOWN_EVENT, &s, &old) != 0)
				{
					return FALSE;
				}
				if (old.sa_handler != SIG_DFL && old.sa_sigaction != handleThat)
				{
					savedTerm = old;
				}
			}
		}
		else
		{
			Handler* prev = nullptr;
			for (Handler* itr = handlers; itr != nullptr; itr = itr->next)
			{
				if (itr->routine == HandlerRoutine)
				{
					if (prev == nullptr)
					{
						handlers = itr->next;
					}
					else
					{
						prev->next = itr->next;
					}
					delete itr;
					if (handlers == nullptr)
					{
						// Uninstall signal handlers
						if (sigaction(CTRL_C_EVENT, &savedInt, nullptr) != 0)
						{
							return FALSE;
						}
						if (sigaction(CTRL_BREAK_EVENT, &savedQuit, nullptr) != 0)
						{
							return FALSE;
						}
						if (sigaction(CTRL_CLOSE_EVENT, &savedHup, nullptr) != 0)
						{
							return FALSE;
						}
						if (sigaction(CTRL_SHUTDOWN_EVENT, &savedTerm, nullptr) != 0)
						{
							return FALSE;
						}
					}
					return TRUE;
				}
				prev = itr;
			}
			// Handler to remove not found
			errno = ERROR_INVALID_PARAMETER;
			return FALSE;
		}
	}
	return TRUE;
}

BOOL GenerateConsoleCtrlEvent(DWORD dwCtrlEvent, DWORD dwProcessGroupId)
{
	if (killpg(dwProcessGroupId, dwCtrlEvent) != 0)
	{
		return FALSE;
	}
	return TRUE;
}
