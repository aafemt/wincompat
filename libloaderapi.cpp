#include <string>
#include <string.h>
#include <unistd.h>

#include "libloaderapi.h"
#include "winerror.h"

namespace
{
	// TODO: Add fallback to readenv("_") if /proc/self is not accessible
	size_t getExeName(char* buffer, size_t length)
	{
		int exe_name_len = readlink("/proc/self/exe", buffer, length);
		if (exe_name_len < 0)
		{
			return 0;
		}
		return exe_name_len;
	}

	void* tryLoad(const std::string& fileName, const std::string_view& exePath)
	{
		void* result = dlopen(fileName.c_str(), RTLD_LAZY | RTLD_LOCAL);
		if (result == nullptr && fileName[0] != '/' && exePath.size() != 0)
		{
			// relative path
			std::string newFileName(exePath);
			newFileName += fileName;
			result = dlopen(newFileName.c_str(), RTLD_LAZY | RTLD_LOCAL);
		}
		return result;
	}
}

HMODULE LoadLibrary(const char* lpFileName)
{
	// First of all try to load the library "as is".
	// this is a little against specifications quoted below
	void* result = dlopen(lpFileName, RTLD_LAZY | RTLD_LOCAL);
	if (result != nullptr)
	{
		return result;
	}

	// MSDN:
	// If the string specifies a module name without a path and the file name extension
	// is omitted, the function appends the default library extension ".DLL" to the module name.
	// To prevent the function from appending ".DLL" to the module name, include a trailing
	// point character (.) in the module name string.

	// Despite of that, Windows doesn't care about part "without a path".

	std::string fileName(lpFileName);
	size_t slash = fileName.find_last_of('/');
	if (slash == fileName.npos)
	{
		// Search whole name
		slash = 0;
	}
	if (fileName.find_first_of('.', slash) == fileName.npos)
	{
		// On Linux extension is naturally ".so".
		fileName += ".so";
	}
	// Windows search path includes executable directory as well
	char buffer[MAX_PATH];
	size_t exeLen = getExeName(buffer, sizeof(buffer));
	// strrchr is just a waste of time, besides name is not null-terminated
	while (exeLen > 0 && buffer[exeLen - 1] != '/')
	{
		--exeLen;
	}
	const std::string_view exePath(buffer, exeLen);

	result = tryLoad(fileName, exePath);
	if (result == nullptr)
	{
		// Let's be more compatible, try also to add 'lib' prefix
		// Yes, there is a theoretical possibility that target library is in root directory
		if (slash == 0 && fileName[0] != '/')
		{
			fileName.insert(0, "lib");
		}
		else
		{
			fileName.insert(slash + 1, "lib");
		}
		result = tryLoad(fileName, exePath);
	}
	if (result == nullptr)
	{
		// Despite of all these _set_errno() calls inside of dlopen(), it fail
		// to set errno to a meaningful value. Funny.
		errno = ERROR_MOD_NOT_FOUND;
	}
	return result;
}

HMODULE LoadLibrary(const wchar_t* lpFileName)
{
    char name[MAX_PATH];
    if (wcstombs(name, lpFileName, sizeof(name)) >= sizeof(name))
    {
        return nullptr;
    }
    return LoadLibrary(name);
}

DWORD GetModuleFileName(HMODULE hModule, char* lpFileName, DWORD nSize)
{
	if (hModule == nullptr)
	{
		size_t exe_name_len = getExeName(lpFileName, nSize);
		if (exe_name_len == 0)
		{
			return 0;
		}
		if (exe_name_len == nSize)
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
