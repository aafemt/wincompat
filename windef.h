/*********** Misc defines ****************/

#pragma once

#ifdef __WIN32__
#error This file is not for Windows!
#endif // __WIN32__

#include <limits.h>

// Do we really need compatibility of calling conventions...?
// Perhaps yes because user routines can be declared "stdcall" explicitly and this must cause compilation error
#ifdef __x86_64__
#define WINAPI
#else
#define WINAPI __attribute__((__stdcall__))
#endif // __x86_64__

#define APIENTRY WINAPI

typedef int WINBOOL;
typedef int BOOL;
// It is ABI incompatibility, but we don't care. The main purpose is source compatibility
// and damned DWORD on Windows is defined as "unsigned long int" that makes format strings crazy.
typedef unsigned long int DWORD;
typedef DWORD* LPDWORD;
typedef void* HINSTANCE;
typedef HINSTANCE HMODULE;
typedef void* HWND;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef void* LPVOID;

inline WORD MAKEWORD(BYTE a, BYTE b) { return a | static_cast<WORD>(b) << 8; }
inline BYTE LOBYTE(WORD w) { return w & 0xff; }
inline BYTE HIBYTE(WORD w) { return (w >> 8) & 0xff; }

#define TRUE 1
#define FALSE 0

#define MAX_PATH PATH_MAX
