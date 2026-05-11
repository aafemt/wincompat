#pragma once

#include_next <string.h>

int strcat_s(char* strDestination, size_t numberOfElements, const char* strSource);
int _wcsnicmp(const wchar_t* string1, const wchar_t* string2, size_t count);
