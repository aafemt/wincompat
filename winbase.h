#pragma once
#include <stdlib.h>

#include "windef.h"
#include "winnt.h"
#include "handleapi.h"
#include "processthreadsapi.h"
#include "securitybaseapi.h"

#define FILE_BEGIN      0   // SEEK_SET
#define FILE_CURRENT    1   // SEEK_CUR
#define FILE_END        2   // SEEK_END

constexpr DWORD FILE_FLAG_DELETE_ON_CLOSE = 0x4000000;
constexpr DWORD FILE_FLAG_SEQUENTIAL_SCAN = 0x8000000;

struct FILE_END_OF_FILE_INFO
{
    LARGE_INTEGER EndOfFile;
};

struct OVERLAPPED
{
//    struct
//    {
        // No DWORD here because of 64 bits size
        unsigned int Offset = 0;
        unsigned int OffsetHigh = 0;
//    };
    //HANDLE hEvent = INVALID_HANDLE_VALUE;
};

struct SECURITY_ATTRIBUTES
{
	DWORD nLength;
	void* lpSecurityDescriptor;
	WINBOOL bInheritHandle;
};
typedef SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;

struct FILETIME
{
	unsigned dwLowDateTime;
	unsigned dwHighDateTime;
};

struct WIN32_FIND_DATAA
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	unsigned int nFileSizeHigh;
	unsigned int nFileSizeLow;
	char cFileName[NAME_MAX];
	char cAlternateFileName[14];
};

struct WIN32_FIND_DATAW
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	unsigned int nFileSizeHigh;
	unsigned int nFileSizeLow;
	wchar_t cFileName[NAME_MAX];
	wchar_t cAlternateFileName[14];
};

#ifdef UNICODE
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
#else
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;
#endif // UNICODE

typedef void* HLOCAL;

inline HLOCAL LocalFree(HLOCAL hMem)
{
	if (hMem != nullptr)
		free(hMem);
	return nullptr;
}
