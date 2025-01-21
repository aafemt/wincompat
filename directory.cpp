/*******************************************************
*
* Implementation of Create/RemoveDirectory functions
*
********************************************************/

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileapi.h"

WINBOOL CreateDirectory(const char* lpPathName, [[maybe_unused]]SECURITY_ATTRIBUTES* lpSecurityAttributes)
{
    if (mkdir(lpPathName, 0777) == 0) // For now request full access. TODO: recognize lpSecurityAttributes
    {
        return TRUE;
    }
    return FALSE;
}

WINBOOL CreateDirectory(const wchar_t* lpPathName, [[maybe_unused]]SECURITY_ATTRIBUTES* lpSecurityAttributes)
{
    char name[PATH_MAX];
    if (wcstombs(name, lpPathName, sizeof(name)) >= sizeof(name))
    {
        return FALSE;
    }
    if (mkdir(name, 0777) == 0) // For now request full access. TODO: recognize lpSecurityAttributes
    {
        return TRUE;
    }
    return FALSE;
}

WINBOOL RemoveDirectory(const char* lpPathName)
{
	if (rmdir(lpPathName) == 0)
	{
		return TRUE;
	}
	return FALSE;
}

WINBOOL RemoveDirectory(const wchar_t* lpPathName)
{
    char name[PATH_MAX];
    if (wcstombs(name, lpPathName, sizeof(name)) >= sizeof(name))
    {
        return FALSE;
    }
	if (rmdir(name) == 0)
	{
		return TRUE;
	}
	return FALSE;
}
