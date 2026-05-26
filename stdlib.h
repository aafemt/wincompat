#pragma once

#include_next <stdlib.h>
#include <stdint.h>
#include <locale.h>

// GLIBC has implementation inside but doesn't publish it (yet?).
// Use our own implementation.
size_t _mbstowcs_l(wchar_t* wcstr, const char* mbstr, size_t n, locale_t locale);

inline uint32_t _byteswap_ulong(uint32_t val)
{
	return __builtin_bswap32(val);
}
