#pragma once

#include "windef.h"
#include "winnt.h"
#include "winuser.h"

const int CSIDL_FLAG_CREATE		= 0x8000;
const int CSIDL_FLAG_MASK		= 0xff00;
const int CSIDL_COMMON_APPDATA	= 0x0023;

const DWORD SHGFP_TYPE_CURRENT	= 0;
const DWORD SHGFP_TYPE_DEFAULT	= 1;

HRESULT SHGetFolderPath(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, char* pszPath);

