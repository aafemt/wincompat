#include <windows.h>

namespace
{
	HMODULE thisInstance = nullptr;
}

extern "C"
{
	__attribute__((visibility("protected"))) HMODULE getHandle();
	HMODULE getHandle()
	{
		return thisInstance;
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID info)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		{
			thisInstance = module;
			break;
		}
/*
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
*/
	case DLL_PROCESS_DETACH:
		{
			(void)(info == nullptr);
			break;
		}
	}
	return TRUE;
}
