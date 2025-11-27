/***********************************************************
*
*  Tests for path functions
*
************************************************************/

#include <windows.h>
#include <shlwapi.h>

#include "globals.h"

void checkFileName(const char* path, const char* filename)
{
	const char* result = PathFindFileNameA(path);
	if (strcmp(result, filename) != 0)
	{
		failure("PathFindFileName(%s) == %s instead of %s\n", path, result, filename);
	}
}

void checkIsDirectory(const char* path, WINBOOL expectedResult)
{
	WINBOOL result = PathIsDirectoryA(path);
	if (result != expectedResult)
	{
		failure("PathIsDirectory(%s) returned %d instead of %d\n", path, result, expectedResult);
	}
}

void RunPathTests()
{
	checkFileName("/path/file.ext", "file.ext");
	checkFileName("/path/file", "file");
	checkFileName("/path/", "path/");
	checkFileName("/path", "path");
	checkFileName("/", "/");
	checkFileName("/path/.ext", ".ext");
	checkFileName(".", ".");
	checkFileName("file.ext", "file.ext");

	// Let's use executable name as an existing non-direcctory
	char buffer[MAX_PATH];
	if (GetModuleFileNameA(NULL, buffer, sizeof(buffer)) == 0)
	{
		failure("GetModuleFileName returned error %lu\n", GetLastError());
		return;
	}
	checkIsDirectory(buffer, FALSE);
	DWORD l = GetCurrentDirectoryA(sizeof(buffer), buffer);
	if (l == 0)
	{
		failure("GetCurrentDirectory returned error %lu\n", GetLastError());
		return;
	}
	checkIsDirectory(buffer, (WINBOOL)FILE_ATTRIBUTE_DIRECTORY);
	// Force separator at the end and try again, result must be the same
	buffer[l++] = '/';
	buffer[l] = '\0';
	checkIsDirectory(buffer, (WINBOOL)FILE_ATTRIBUTE_DIRECTORY);
	// Valid but non-existing path must return FALSE
	buffer[l++] = 'n';
	buffer[l] = '\0';
	checkIsDirectory(buffer, FALSE);
	// ...even if ended with separator
	buffer[l++] = '/';
	buffer[l] = '\0';
	checkIsDirectory(buffer, FALSE);
}
