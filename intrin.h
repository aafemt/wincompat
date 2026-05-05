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

inline void __cpuidex(int cpuInfo[4], int function_id, int subfunction_id)
{
	__cpuid_count(function_id, subfunction_id, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
}

#pragma GCC target("sse2")
#include <tmmintrin.h>
#pragma GCC target("sha,sse4.1")
#include <immintrin.h>
