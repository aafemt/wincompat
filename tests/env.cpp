/*******************************************************
*
*  Tests for functions working with environment
*
********************************************************/

#define WIN32_LEAN_AND_MEAN
#include <string_view>
#include <windows.h>

#include "globals.h"

void RunEnvTests()
{
	// Test ANSI->Wide conversion
	if (SetEnvironmentVariableA("TestAnsiVar", "AnsiText") == FALSE)
	{
		failure("SetEnvironmentVariableA returned error %lu\n", GetLastError());
	}
	wchar_t wbuffer[20];
	DWORD res = GetEnvironmentVariableW(L"TestAnsiVar", wbuffer, 20);
	if (res != 8) //std::size("AnsiText");
	{
		failure("GetEnvironmentVariableW returned %lu instead of 8, error %lu\n", res, GetLastError());
	}
	if (std::wstring_view(L"AnsiText") != wbuffer)
	{
		failure("GetEnvironmentVariableW returned %S instead of 'AnsiText'\n", wbuffer);
	}

	// Test Wide->ANSI conversion
	if (SetEnvironmentVariableW(L"TestWideVar", L"WideText") == FALSE)
	{
		failure("SetEnvironmentVariableW returned error %lu\n", GetLastError());
	}
	char abuffer[20];
	res = GetEnvironmentVariableA("TestWideVar", abuffer, 20);
	if (res != 8) //std::size("WideText");
	{
		failure("GetEnvironmentVariableA returned %lu instead of 8, error %lu\n", res, GetLastError());
	}
	if (std::string_view("WideText") != abuffer)
	{
		failure("GetEnvironmentVariableA returned %s instead of 'WideText'\n", abuffer);
	}

	// Test full Unicode power
	// Assume that current locale uses UTF-8 charset for now. May be it worth to check it to be sure.
	if (SetEnvironmentVariableW(L"TestWideVar", L"AbcАбвěšč") == FALSE)
	{
		failure("SetEnvironmentVariableW returned error %lu\n", GetLastError());
	}
	res = GetEnvironmentVariableW(L"TestWideVar", wbuffer, 20);
	if (res != 9) //std::size("AnsiText");
	{
		failure("GetEnvironmentVariableW returned %lu instead of 9, error %lu\n", res, GetLastError());
	}
	if (std::wstring_view(L"AbcАбвěšč") != wbuffer)
	{
		failure("GetEnvironmentVariableW returned %S instead of unicode text\n", wbuffer);
	}

	// Test variable erasing
	if (SetEnvironmentVariableA("TestWideVar", nullptr) == FALSE)
	{
		failure("Erasing variable returned error %lu\n", GetLastError());
	}

	// Test that variable is erased and returned error
	res = GetEnvironmentVariableA("TestWideVar", abuffer, 20);
	DWORD err = GetLastError();
	if (res != 0)
	{
		failure("Getting of non-existing variable returned %lu instead of zero\n", res);
	}
	else if (err != ERROR_ENVVAR_NOT_FOUND)
	{
		failure("Getting of non-existing variable returned error %lu instead of ERROR_ENVVAR_NOT_FOUND\n", err);
	}
	res = GetEnvironmentVariableW(L"TestWideVar", wbuffer, 20);
	err = GetLastError();
	if (res != 0)
	{
		failure("Getting of non-existing variable as wide string returned %lu instead of zero\n", res);
	}
	else if (err != ERROR_ENVVAR_NOT_FOUND)
	{
		failure("Getting of non-existing variable as wide string returned error %lu instead of ERROR_ENVVAR_NOT_FOUND\n", err);
	}

	// Test buffer overflow
	res = GetEnvironmentVariableA("TestAnsiVar", abuffer, 2);
	if (res != 9)
	{
		failure("GetEnvironmentVariableA returned %lu instead of 9 for small buffer, error %lu\n", res, GetLastError());
	}

}
