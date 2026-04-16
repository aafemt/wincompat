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
	else
	{
		FreeLibrary(lib);
	}
	lib = LoadLibrary("." DIR_SEP "testdll");
	if (lib == nullptr)
	{
		failure("Test DLL without extension failed to load\n");
		return;
	}
	decltype(getHandle)* func = reinterpret_cast<decltype(getHandle)*>(GetProcAddress(lib, "getHandle"));
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
	}
	FreeLibrary(lib);
}
