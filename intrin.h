#pragma once

#ifdef __WIN32__
#error This file is not for Windows
#endif // __WIN32__

#include <cpuid.h>

// cpuid.h defines __cpuid() as a macro. That makes some troubles
inline void __cpuid_impl(int cpuInfo[4], int function_id)
{
	__cpuid(function_id, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

#undef __cpuid
inline void __cpuid(int cpuInfo[4], int function_id)
{
	__cpuid_impl(cpuInfo, function_id);
}
