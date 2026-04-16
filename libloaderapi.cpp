#include <string>
#include <string.h>
#include <unistd.h>

#include "libloaderapi.h"
#include "winerror.h"

HMODULE LoadLibrary(const char* lpFileName)
{
	// MSDN:
	// If the string specifies a module name without a path and the file name extension
	// is omitted, the function appends the default library extension ".DLL" to the module name.
	// To prevent the function from appending ".DLL" to the module name, include a trailing
	// point character (.) in the module name string.

	// Despite of that, Windows doesn't care about part "without a path".

	const char* slash = strrchr(lpFileName, '/');
	if (slash == nullptr)
	{
		// Search whole name
		slash = lpFileName;
	}
	if (strchr(slash, '.') == nullptr)
	{
		std::string newName(lpFileName);
		// On Linux extension is naturally ".so".
		newName += ".so";
		return dlopen(newName.c_str(), RTLD_LAZY | RTLD_LOCAL);
	}
	return dlopen(lpFileName, RTLD_LAZY | RTLD_LOCAL);
}

DWORD GetModuleFileName(HMODULE hModule, char* lpFileName, DWORD nSize)
{
	if (hModule == nullptr)
	{
		int exe_name_len = readlink("/proc/self/exe", lpFileName, nSize);
		if (exe_name_len < 0)
		{
			return 0;
		}
		if ((DWORD)exe_name_len == nSize)
		{
			// GetModuleFileName has to terminate buffer so if it is bigger - truncation happen
			lpFileName[nSize - 1] = '\0';
			errno = ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			lpFileName[exe_name_len] = '\0';
		}
		return exe_name_len;
	}
	else
	{
		errno = ERROR_CALL_NOT_IMPLEMENTED;
		return 0;
	}
}
