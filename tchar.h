#pragma once

#ifdef _UNICODE
#include <wchar.h>

	typedef wchar_t _TCHAR;
	typedef wchar_t TCHAR;

	#define _TEXT(x) L##x

	#define _tmain wmain
	#define _topen _wopen
#else
#include <string.h>

	typedef char _TCHAR;
	typedef char TCHAR;

	#define _TEXT(x) x

	#define _tmain main
	#define _topen open
#endif // _UNICODE
