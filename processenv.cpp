/**************************************************
*
* Implementation of environment-related functions
*
****************************************************/

#include <memory>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "winbase.h"
#include "winerror.h"

DWORD GetCurrentDirectory(DWORD nBufferLength, char* lpBuffer)
{
	char* value = get_current_dir_name();
	if (value == nullptr)
	{
		return 0;
	}

	size_t len = strlen(value);
	if (len < nBufferLength)
	{
		strcpy(lpBuffer, value);
	}
	else
	{
		// "If the buffer thet is pointed to by lpBuffer is not large enough, the return value specifies
		// the required size of the buffer, in characters, including the null-terminating character."
		++len;
		// It is strange, but GetCurrentDirectory and other functions here doen't set ERROR_BUFFER_OVERFLOW
	}
	free(value);

	return len;
}

DWORD GetEnvironmentVariable(const char* lpName, char* lpBuffer, DWORD nSize)
{
	const char* value = getenv(lpName);
	if (value == nullptr)
	{
		errno = ERROR_ENVVAR_NOT_FOUND;
		return 0;
	}

	size_t len = strlen(value);
	if (len < nSize)
	{
		strcpy(lpBuffer, value);
	}
	else
	{
		++len;
	}

	return len;
}

DWORD GetEnvironmentVariable(const wchar_t* lpName, wchar_t* lpBuffer, DWORD nSize)
{
	size_t len = wcslen(lpName);
	const size_t bufLen = (len + 1) * 4;
	std::unique_ptr<char[]> buffer(new char[bufLen]);
	size_t res = wcstombs(buffer.get(), lpName, bufLen);
	if (res == (size_t)-1)
	{
		// I wonder if wcstombs set errno
		return 0;
	}

	const char* value = getenv(buffer.get());
	if (value == nullptr)
	{
		errno = ERROR_ENVVAR_NOT_FOUND;
		return 0;
	}

	len = strlen(value);
	// "If lpBuffer is not large enough to hold the data, the return value is the buffer size,
	// in characters, required to hold the string and its terminating null character
	// and the contents of lpBuffer are undefined."
	size_t wlen = mbstowcs(nullptr, value, 0);
	if (wlen < nSize)
	{
		mbstowcs(lpBuffer, value, nSize);
	}
	else
	{
		++wlen;
	}

	return wlen;
}

BOOL SetEnvironmentVariable(const char* lpName, const char* lpValue)
{
	if (lpValue == nullptr)
	{
		return unsetenv(lpName) == 0 ? TRUE : FALSE;
	}
	return setenv(lpName, lpValue, 1) == 0 ? TRUE : FALSE;
}

BOOL SetEnvironmentVariable(const wchar_t* lpName, const wchar_t* lpValue)
{
	size_t len = wcslen(lpName);
	size_t bufLen = (len + 1) * 4;
	std::unique_ptr<char[]> buffer(new char[bufLen]);
	size_t res = wcstombs(buffer.get(), lpName, bufLen);
	if (res == (size_t)-1)
	{
		// I wonder if wcstombs set errno
		return 0;
	}

	if (lpValue == nullptr)
	{
		return unsetenv(buffer.get()) == 0 ? TRUE : FALSE;
	}

	len = wcslen(lpValue);
	bufLen = (len + 1) * 4;
	std::unique_ptr<char[]> valueBuffer(new char[bufLen]);
	res = wcstombs(valueBuffer.get(), lpValue, bufLen);
	if (res == (size_t)-1)
	{
		// I wonder if wcstombs set errno
		return 0;
	}

	return setenv(buffer.get(), valueBuffer.get(), 1) == 0 ? TRUE : FALSE;
}
