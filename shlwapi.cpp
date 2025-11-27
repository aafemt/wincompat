/*****************************************************
*
*  Implementation of functions declared in shlwapi.h
*
******************************************************/

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wchar.h>

#include "winnt.h"
#include "shlwapi.h"

const char* PathFindFileName(const char* pszPath)
{
	// Because crazy Windows returns "path/" from "/path/", strrchr is not useable here
	// For the same reasoon skip the last character. Even if it is a separator - no count.
	const char* slash = strchr(pszPath, '\0') - 1;
	while (--slash >= pszPath)
	{
		if (*slash == '/')
			return ++slash;
	}

	return pszPath;
}

const wchar_t* PathFindFileName(const wchar_t* pszPath)
{
	const wchar_t* slash = wcschr(pszPath, L'\0') - 1;
	while (--slash >= pszPath)
	{
		if (*slash == L'/')
			return ++slash;
	}

	return pszPath;
}

WINBOOL PathIsDirectory(const char* pszPath)
{
	struct stat st = {};
	if (stat(pszPath, &st) != 0)
	{
		// Windows has no way to return error
		return FALSE;
	}
	return (st.st_mode & S_IFDIR) != 0 ? FILE_ATTRIBUTE_DIRECTORY : FALSE;
}

WINBOOL PathIsDirectory(const wchar_t* pszPath)
{
	char path[PATH_MAX];
	if (wcstombs(path, pszPath, sizeof(path)) == (size_t)-1)
	{
		return FALSE;
	}
	return PathIsDirectory(path);
}
