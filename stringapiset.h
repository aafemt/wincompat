#pragma once

#include "windef.h"

int WideCharToMultiByte(unsigned CodePage, DWORD dwFlags,
						const wchar_t* lpWideCharStr, int cchWideChar,
						char* lpMultiByteStr, int cbMultiByte,
						const char* lpDefaultChar, BOOL* lpUsedDefaultChar);
