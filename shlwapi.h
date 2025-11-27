#pragma once

#include <string.h>
#include "windef.h"

inline WINBOOL PathIsRelative(const char* lpszPath)
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

const wchar_t* PathFindFileName(const wchar_t* pszPath);
const char* PathFindFileName(const char* pszPath);
inline const char* PathFindFileNameA(const char* pszPath)
{
	return PathFindFileName(pszPath);
}
inline const wchar_t* PathFindFileNameW(const wchar_t* pszPath)
{
	return PathFindFileName(pszPath);
}

WINBOOL PathIsDirectory(const wchar_t* pszPath);
WINBOOL PathIsDirectory(const char* pszPath);
inline WINBOOL PathIsDirectoryA(const char* pszPath)
{
	return PathIsDirectory(pszPath);
}
inline WINBOOL PathIsDirectoryW(const char* pszPath)
{
	return PathIsDirectory(pszPath);
}
