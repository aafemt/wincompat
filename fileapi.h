#pragma once

#include <bits/types/FILE.h>
#include "winbase.h"
#include "windef.h"
#include "winnt.h"

// Very incomplete
enum FILE_INFO_BY_HANDLE_CLASS
{
	FileDispositionInfo = 4,
    FileEndOfFileInfo = 6,
    MaximumFileInfoByHandleClass
};

constexpr DWORD CREATE_NEW		= 1;
constexpr DWORD CREATE_ALWAYS	= 2;
constexpr DWORD OPEN_EXISTING	= 3;
constexpr DWORD OPEN_ALWAYS		= 4;
constexpr DWORD TRUNCATE_EXISTING	= 5;

#define INVALID_SET_FILE_POINTER ((DWORD)-1)

FILE* _wfopen(const wchar_t* filename, const wchar_t* mode);

HANDLE CreateFile(const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAtrributes, HANDLE hTemplateFile);
HANDLE CreateFile(const wchar_t* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAtrributes, HANDLE hTemplateFile);
inline HANDLE CreateFileA(const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAtrributes, HANDLE hTemplateFile)
{
	return CreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAtrributes, hTemplateFile);
}
inline HANDLE CreateFileW(const wchar_t* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAtrributes, HANDLE hTemplateFile)
{
	return CreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAtrributes, hTemplateFile);
}

WINBOOL ReadFile(HANDLE hFile, void* lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, OVERLAPPED* lpOverlapped);
WINBOOL WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, OVERLAPPED* lpOverlapped);
WINBOOL FlushFileBuffers(HANDLE hFile);
DWORD SetFilePointer(HANDLE hFile, int32_t lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod);
WINBOOL SetFilePointerEx(HANDLE hFile, const LARGE_INTEGER& liDistanceToMove, LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod);
WINBOOL SetFileInformationByHandle(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, void* lpFileInformation, DWORD dwBufferSize);
WINBOOL SetEndOfFile(HANDLE hFile);
WINBOOL GetFileSizeEx(HANDLE hFile, LARGE_INTEGER* lpFileSize);
WINBOOL DeleteFile(const char* lpFileName);
WINBOOL DeleteFile(const wchar_t* lpFileName);

DWORD GetTempPath(DWORD nBufferLength, char* lpBuffer);
DWORD GetTempPath(DWORD nBufferLength, wchar_t* lpBuffer);
inline DWORD GetTempPathA(DWORD nBufferLength, char* lpBuffer)
{
    return GetTempPath(nBufferLength, lpBuffer);
}
inline DWORD GetTempPathW(DWORD nBufferLength, wchar_t* lpBuffer)
{
    return GetTempPath(nBufferLength, lpBuffer);
}

WINBOOL CreateDirectory(const char* lpPathName, SECURITY_ATTRIBUTES* lpSecurityAttributes);
WINBOOL CreateDirectory(const wchar_t* lpPathName, SECURITY_ATTRIBUTES* lpSecurityAttributes);

WINBOOL RemoveDirectory(const char* lpPathName);
WINBOOL RemoveDirectory(const wchar_t* lpPathName);

HANDLE FindFirstFile(const char* lpFileName, WIN32_FIND_DATAA* lpFindFileData);
HANDLE FindFirstFile(const wchar_t* lpFileName, WIN32_FIND_DATAW* lpFindFileData);
WINBOOL FindNextFile(HANDLE hFindFile, WIN32_FIND_DATAA* lpFindFileData);
WINBOOL FindNextFile(HANDLE hFindFile, WIN32_FIND_DATAW* lpFindFileData);
WINBOOL FindClose(volatile HANDLE& hFindFile);
