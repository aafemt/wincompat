#pragma once

#include <dlfcn.h>

#include "windef.h"

HMODULE LoadLibrary(const char* lpFileName);
HMODULE LoadLibrary(const wchar_t* lpFileName);
inline HMODULE LoadLibraryA(const char* lpFileName)
{
	return LoadLibrary(lpFileName);
}
inline HMODULE LoadLibraryW(const wchar_t* lpFileName)
{
	return LoadLibrary(lpFileName);
}
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
