#pragma once

#include <stdio.h>

inline bool failed = false;
inline unsigned failures = 0;

#define failure(FMT, ...) { printf(FMT, ##__VA_ARGS__); failed = true; failures++; }
