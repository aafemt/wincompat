#pragma once

#include <dlfcn.h>

#include "windef.h"

HMODULE LoadLibrary(const char* lpFileName);
inline void* GetProcAddress(HMODULE hModule, const char* lpProcName)
{
    return dlsym(hModule, lpProcName);
}
inline WINBOOL FreeLibrary(HMODULE hModule) { return dlclose(hModule) == 0?TRUE:FALSE; }
DWORD GetModuleFileName(HMODULE hModule, char* lpFileName, DWORD nSize);
inline DWORD GetModuleFileNameA(HMODULE hModule, char* lpFileName, DWORD nSize)
{
	return GetModuleFileName(hModule, lpFileName, nSize);
}
