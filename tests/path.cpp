/***********************************************************
*
*  Tests for path functions
*
************************************************************/

#include <windows.h>
#include <shlwapi.h>
#include <pathcch.h>
#include <wchar.h>

#include "globals.h"

void checkFileName(const char* path, const char* filename)
{
	const char* result = PathFindFileNameA(path);
	if (strcmp(result, filename) != 0)
	{
		failure("PathFindFileName(%s) == %s instead of %s\n", path, result, filename);
	}
}

void checkRemoveFileName(const wchar_t* path, const wchar_t* result, HRESULT returnCode = S_OK)
{
	wchar_t tmp[1024];
	wcscpy(tmp, path);
	HRESULT res = PathCchRemoveFileSpec(tmp, sizeof(tmp));
	if (res != returnCode)
	{
		failure("PathCchRemoveFileSpec returned %lx instead of S_OK\n", res);
	}
	else if (wcscmp(tmp, result) != 0)
	{
		failure("PathCchRemoveFileSpec returned %S instead of %S\n", tmp, result);
	}
}

void checkAddBackslash(const wchar_t* path, size_t length, HRESULT expectedReturn, const wchar_t* expectedResult)
{
	wchar_t tmp[length + 1];
	tmp[length] = L'\xdead';
	wcsncpy(tmp, path, length);
	HRESULT res = PathCchAddBackslash(tmp, length);
	if (tmp[length] != L'\xdead')
	{
		failure("PathCchAddBackslash run aout of buffer\n");
	}
	if (res != expectedReturn)
	{
		failure("PathCchAddBackslash returned %lx instead of %lx\n", res, expectedReturn);
	}
	else if (expectedResult != nullptr && wcscmp(tmp, expectedResult) != 0)
	{
		failure("PathCchAddBackslash returned %S instead of %S\n", tmp, expectedResult);
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

	// PathCchRemoveFileName doesn't recognize slash as a dir separator on Windows
	checkRemoveFileName(DIR_SEP L"path1", DIR_SEP);
	checkRemoveFileName(DIR_SEP L"path1" DIR_SEP "path2", DIR_SEP L"path1");
	checkRemoveFileName(DIR_SEP L"path1" DIR_SEP, DIR_SEP L"path1");
	checkRemoveFileName(DIR_SEP, DIR_SEP, S_FALSE);
	checkRemoveFileName(L"no path", L"");
	HRESULT res = PathCchRemoveFileSpec(nullptr, 1);
	if (res != E_INVALIDARG)
	{
		failure("PathCchRemoveFileSpec(nullptr) returned %lx instead of E_INVALIDARG\n", res);
	}
	wchar_t wbuffer[1024];
	res = PathCchRemoveFileSpec(wbuffer, 0);
	if (res != E_INVALIDARG)
	{
		failure("PathCchRemoveFileSpec(0) returned %lx instead of E_INVALIDARG\n", res);
	}

	checkAddBackslash(L"path", 10, S_OK, L"path" DIR_SEP);
	checkAddBackslash(L"path" DIR_SEP, 10, S_FALSE, L"path" DIR_SEP);
	// Corner cases
	checkAddBackslash(L"path" DIR_SEP, 0, E_NOT_SUFFICIENT_BUFFER, nullptr);
	checkAddBackslash(L"path" DIR_SEP, 3, E_NOT_SUFFICIENT_BUFFER, nullptr);
	checkAddBackslash(L"path" DIR_SEP, 5, E_NOT_SUFFICIENT_BUFFER, nullptr); // This case is funny because buffer IS enough
	checkAddBackslash(L"", 5, S_FALSE, L""); // This case is even more funny

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
