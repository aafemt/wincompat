#include <unistd.h>

#include "libloaderapi.h"
#include "winerror.h"

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
