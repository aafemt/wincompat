/*****************************************************
*
*  Call DllMain() for shared libraries
*
*  IMPORTANT:
*     On Windows DllMain is called after every global
*     object is initialized.
*
*     Due to unfortunate choice of default init_priority
*     value there is no way to get the same result on
*     Linux except putting wcdllmain.o to the LAST position
*     in linker list.
*
*     Also it is very recommended to use -fno-undefined
*     linker option to make sure that right DllMain is
*     called or error is raised if it is not defined.
*
*     Implementation rely on fact that dlopen() actually
*     return the address of link_map structure for the
*     library. If GLIBC is ever changed (again)...
*
*
*     Dynamically loaded libraries are unloaded before end
*     of host application so there is no way to provide
*     non-null value for info. And this is actually not needed
*     because pthreads are not killed before DllMain call
*     so unloading happen as if FreeLibrary() was called by
*     appliction explicitly.
*
******************************************************/

#include <link.h>
#include <dlfcn.h>

#include "winnt.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID info);

namespace
{
	// Hide initializer as good as possible
	HMODULE getDllHandle()
	{
		link_map* map = nullptr;
		Dl_info info = {};
		dladdr1(reinterpret_cast<const void*>(&getDllHandle), &info, reinterpret_cast<void**>(&map), RTLD_DL_LINKMAP);
		// There is nothng to do on error.
		return map;
	}

	struct wsDllMain
	{
		HMODULE thisDll = nullptr;
		wsDllMain()
		{
			thisDll = getDllHandle();
			// Loader call functions from .init_array as void(*)(int, char**, char**)
			// so returned from DllMain value is ignored.
			// Throwing an exception from here would make whole application terminated.
			(void)DllMain(thisDll, DLL_PROCESS_ATTACH, nullptr);
		}
		~wsDllMain()
		{
			// On Windows info has value 1 if DLL is unloaded as a part of application unload.
			// At this point all background threads are already terminated.
			// If the library is unloaded by FreeLibrary, info is nullptr and background
			// threads still running.
			// On Linux there is no difference, in both cases background threads are
			// still running (and actually there is no special handling of libraries
			// during application unload), so nullptr is used unconditionally.
			(void)DllMain(thisDll, DLL_PROCESS_DETACH, nullptr);
		}
	};

	static wsDllMain dllMainExecutor;
}
