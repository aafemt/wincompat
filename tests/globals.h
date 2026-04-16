#pragma once

#include <stdio.h>

#ifdef __WIN32__
#define DIR_SEP L"\\"
#else
#define DIR_SEP L"/"
#endif // __WIN32__

inline bool failed = false;
inline unsigned failures = 0;

#define failure(FMT, ...) { printf(FMT, ##__VA_ARGS__); failed = true; failures++; }
