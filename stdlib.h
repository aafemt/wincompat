#pragma once

#include_next <stdlib.h>

// GLIBC has implementation inside but doesn't publish it (yet?).
// Use our own implementation.
size_t _mbstowcs_l(wchar_t* wcstr, const char* mbstr, size_t n, locale_t locale);
