#include <array>
#include <stdlib.h>
#include <windows.h>
#include <pathcch.h>
#include <wchar.h>

#include "globals.h"

#ifdef __WIN32__
#define DLL_EXT ".dll"
#else
#define DLL_EXT ".so"
#endif // __WIN32__

extern "C"
{
	HMODULE getHandle();
}

void RunDllTests()
{
	HMODULE lib = LoadLibrary("." DIR_SEP "testdll" DLL_EXT);
	if (lib == nullptr)
	{
		failure("Test DLL with extension failed to load\n");
		return;
	}
	HMODULE lib2 = LoadLibrary("." DIR_SEP "testdll");
	if (lib == nullptr)
	{
		failure("Test DLL without extension failed to load\n");
		FreeLibrary(lib);
		return;
	}
	HMODULE lib3 = LoadLibraryA("testdll");
	if (lib == nullptr)
	{
		failure("Test DLL failed to load from executable path\n");
		return;
	}
	else
	{
		FreeLibrary(lib3);
	}
	decltype(getHandle)* func = reinterpret_cast<decltype(getHandle)*>(reinterpret_cast<void*>(GetProcAddress(lib, "getHandle")));
	if (func == nullptr)
	{
		failure("getHandle function not found\n");
	}
	else
	{
		HMODULE libModule = func();
		if (libModule != lib)
		{
			failure("Module in DllMain(%p) is different from returned by LoadLibrary (%p)\n", libModule, lib);
		}
		if (lib != lib2)
		{
			failure("Library loaded with different pathes are different (%p vs %p)\n", lib, lib2);
		}
	}

	wchar_t exeName[PATH_MAX];
	DWORD exeNameLen = GetModuleFileName(nullptr, exeName, std::size(exeName));
	if (exeNameLen == 0 || exeNameLen >= sizeof(exeName))
	{
		failure("GetModuleFileName returned error %ld for executable name\n", GetLastError());
	}
	else
	{
		wchar_t libraryName[PATH_MAX];
		DWORD libraryNameLen = GetModuleFileName(lib, libraryName, std::size(libraryName));
		if (libraryNameLen == 0 || libraryNameLen >= sizeof(libraryName))
		{
			failure("GetModuleFileName returned error %ld for library name\n", GetLastError());
		}
		else
		{
			HRESULT res;
			if ((res = PathCchRemoveFileSpec(exeName, sizeof(exeName))) != S_OK)
			{
				failure("PathCchRemoveFileSpec returned %lx instead of S_OK for executable name %S\n", res, exeName);
			}
			else if ((res = PathCchRemoveFileSpec(libraryName, sizeof(libraryName))) != S_OK)
			{
				failure("PathCchRemoveFileSpec returned %lx instead of S_OK for library name %S\n", res, libraryName);
			}
			else
			{
				if (wcscmp(exeName, libraryName) != 0)
				{
					failure("Executable path %S is not the same as library path %S\n", exeName, libraryName);
				}
			}
		}
	}

	FreeLibrary(lib2);
	FreeLibrary(lib);

	// Test returned error
	lib = LoadLibrary(L"nonexisting");
	if (lib != nullptr)
	{
		failure("Loading of non-existing library returned value\n");
		FreeLibrary(lib);
	}
	else
	{
		DWORD err = GetLastError();
		if (err != ERROR_MOD_NOT_FOUND)
		{
			failure("Loading of non-existing library returned error %ld instead of ERROR_MOD_NOT_FOUND\n", err);
		}
	}
	if (lib != nullptr)
	{
		failure("Loading of library from non-existing path returned value\n");
		FreeLibrary(lib);
	}
	else
	{
		DWORD err = GetLastError();
		if (err != ERROR_MOD_NOT_FOUND)
		{
			failure("Loading of library from non-existing path returned error %ld instead of ERROR_MOD_NOT_FOUND\n", err);
		}
	}
}
