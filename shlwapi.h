#pragma once

#include <string.h>
#include "windef.h"

WINBOOL PathIsRelative(const char* lpszPath)
{
   return *lpszPath == '/'?FALSE:TRUE;
}

inline WINBOOL PathRemoveFileSpec(char* pszPath)
{
	char* slash = strrchr(pszPath, '/');
	if (slash != nullptr)
	{
		*slash = '\0';
		return 1;
	}
	return 0;
}
