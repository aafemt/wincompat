#include <stdlib.h>
#include <windows.h>

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
