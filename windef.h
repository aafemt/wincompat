/*********** Misc defines ****************/

#pragma once

#ifdef __WIN32__
#error This file is not for Windows!
#endif // __WIN32__

#include <limits.h>

typedef int WINBOOL;
// It is ABI incompatibility, but we don't care. The main purpose is source compatibility
// and damned DWORD on Windows is defined as "unsigned long int" that makes format strings crazy.
typedef unsigned long int DWORD;
typedef DWORD* LPDWORD;
typedef void* HMODULE;
typedef void* HWND;
typedef unsigned short WORD;
typedef unsigned char BYTE;

inline WORD MAKEWORD(BYTE a, BYTE b) { return a | static_cast<WORD>(b) << 8; }
inline BYTE LOBYTE(WORD w) { return w & 0xff; }
inline BYTE HIBYTE(WORD w) { return (w >> 8) & 0xff; }

#define TRUE 1
#define FALSE 0

#define MAX_PATH PATH_MAX
