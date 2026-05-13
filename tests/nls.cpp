/*************************************************
*
*   Test for NLS functions
*
**************************************************/
#define WINVER 0x0600
#include <stdio.h>
#include <windows.h>
#include <wchar.h>
#include <wctype.h>

#include "globals.h"

void checkLocaleInfo(LCID locale, LCTYPE value, const wchar_t* expected, DWORD expectError = 0)
{
	wchar_t buffer[80] = {};
	int r = GetLocaleInfoW(locale, value, buffer, 80);
	if (r == 0)
	{
		DWORD err = GetLastError();
		if (err != expectError)
		{
			failure("Failed to get name for locale %u, constant %u. Error %lu, expected %lu\n", (unsigned)locale, (unsigned)value, err, expectError);
		}
		return;
	}
	--r;
	for (int i = 0; i < r; i++)
	{
		buffer[i] = towupper(buffer[i]);
	}
	int expectedLength = wcslen(expected);
	if (r != expectedLength)
	{
		failure("Value length is %d instead of expected %d for %S\n", r, expectedLength, expected);
	}
	if (wcscmp(buffer, expected) != 0)
	{
		failure("Got value '%S' while expected '%S'\n", buffer, expected);
	}
}


void RunNlsTests()
{
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME1, L"JANUARY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME2, L"FEBRUARY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME3, L"MARCH");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME4, L"APRIL");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME5, L"MAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME6, L"JUNE");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME7, L"JULY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME8, L"AUGUST");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME9, L"SEPTEMBER");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME10, L"OCTOBER");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME11, L"NOVEMBER");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SMONTHNAME12, L"DECEMBER");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME1, L"JAN");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME2, L"FEB");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME3, L"MAR");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME4, L"APR");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME5, L"MAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME6, L"JUN");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME7, L"JUL");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME8, L"AUG");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME9, L"SEP");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME10, L"OCT");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME11, L"NOV");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVMONTHNAME12, L"DEC");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME1, L"MONDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME2, L"TUESDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME3, L"WEDNESDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME4, L"THURSDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME5, L"FRIDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME6, L"SATURDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDAYNAME7, L"SUNDAY");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME1, L"MON");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME2, L"TUE");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME3, L"WED");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME4, L"THU");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME5, L"FRI");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME6, L"SAT");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SABBREVDAYNAME7, L"SUN");
//	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SPOSITIVESIGN, L"+"); It may be + or empty string, skip the test
	checkLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SNEGATIVESIGN, L"-");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDECIMAL, L".");
//	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_STHOUSAND, L","); On Linux POSIX locale doesn't define thousand separator
	checkLocaleInfo(0x1111, LOCALE_SABBREVDAYNAME7, L"NO LOCALE", ERROR_INVALID_PARAMETER);

	wchar_t ab[] = { 0x430, 0x431, 0 };
	unsigned char buffer[10];

	// Check that unlimited WideCharToMultibyte correctly terminates target
	memset(buffer, 0x11, sizeof(buffer));
	int res = WideCharToMultiByte(CP_UTF8, 0, ab, -1, reinterpret_cast<char*>(buffer), sizeof(buffer), nullptr, nullptr);
	if (res == 0)
	{
		failure("Zero-terminated WideCharToMultiByte returned error %ld\n", GetLastError());
		return;
	}
	if (res != 5)
	{
		failure("Zero-terminated WideCharToMultiByte returned %d instead of 5\n", res);
	}
	if (buffer[0] != 0xd0 || buffer[1] != 0xb0 ||
		buffer[2] != 0xd0 || buffer[3] != 0xb1 ||
		buffer[4] != 0x00 || buffer[5] != 0x11
		)
	{
		failure("Zero-terminated WideCharToMultiByte result is %x%x%x%x%x%x instead of d0b0d0b10011\n",
				(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2],
				(unsigned char)buffer[3], (unsigned char)buffer[4], (unsigned char)buffer[5])
	}

	// The same for limited
	memset(buffer, 0x11, sizeof(buffer));
	res = WideCharToMultiByte(CP_UTF8, 0, ab, 2, reinterpret_cast<char*>(buffer), sizeof(buffer), nullptr, nullptr);
	if (res == 0)
	{
		failure("Limited WideCharToMultiByte returned error %ld\n", GetLastError());
		return;
	}
	if (res != 4)
	{
		failure("Limited WideCharToMultiByte returned %d instead of 4\n", res);
	}
	if (buffer[0] != 0xd0 || buffer[1] != 0xb0 ||
		buffer[2] != 0xd0 || buffer[3] != 0xb1 ||
		buffer[4] != 0x11 || buffer[5] != 0x11
		)
	{
		failure("Limited WideCharToMultiByte result is %x%x%x%x%x%x instead of d0b0d0b11111\n",
				(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2],
				(unsigned char)buffer[3], (unsigned char)buffer[4], (unsigned char)buffer[5])
	}

	// Check correct calculation of required length
	memset(buffer, 0x11, sizeof(buffer));
	res = WideCharToMultiByte(CP_UTF8, 0, ab, -1, reinterpret_cast<char*>(buffer), 0, nullptr, nullptr);
	if (res == 0)
	{
		failure("Calculating WideCharToMultiByte returned error %ld\n", GetLastError());
		return;
	}
	if (res != 5)
	{
		failure("Calculating WideCharToMultiByte returned %d instead of 5\n", res);
	}
	if (buffer[0] != 0x11)
	{
		failure("Calculating WideCharToMultiByte spoiled result buffer to %x\n", (unsigned char)buffer[0]);
	}
	// It is enough if this call don't crash
	res = WideCharToMultiByte(CP_UTF8, 0, ab, 2, nullptr, 0, nullptr, nullptr);
	if (res != 4)
	{
		failure("Limited calculating WideCharToMultiByte returned %d instead of 4\n", res);
	}
	// Do not test returning ERROR_INVALID_PARAMETER if lpDefaultChar is set for UTF-7/8. This is a pointless limitation.
	// Besides, modern Unicode describes U+FFFD as a relacement charracter for such cases.
	// But test other cases
	res = WideCharToMultiByte(CP_UTF8, 0, ab, 0, reinterpret_cast<char*>(buffer), 0, nullptr, nullptr);
	if (res != 0)
	{
		failure("WideCharToMultiByte with zero input length returned %d instead of zero\n", res);
	}
	else
	{
		DWORD err = GetLastError();
		if (err != ERROR_INVALID_PARAMETER)
		{
			failure("WideCharToMultiByte with zero input length returned error %ld instead of ERROR_INVALID_PARAMETER\n", err);
		}
	}
	res = WideCharToMultiByte(CP_UTF8, 0, nullptr, 2, reinterpret_cast<char*>(buffer), 0, nullptr, nullptr);
	if (res != 0)
	{
		failure("WideCharToMultiByte with zero input returned %d instead of zero\n", res);
	}
	else
	{
		DWORD err = GetLastError();
		if (err != ERROR_INVALID_PARAMETER)
		{
			failure("WideCharToMultiByte with zero input returned error %ld instead of ERROR_INVALID_PARAMETER\n", err);
		}
	}
	res = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<wchar_t*>(buffer), 2, reinterpret_cast<char*>(buffer), sizeof(buffer), nullptr, nullptr);
	if (res != 0)
	{
		failure("WideCharToMultiByte with input==output returned %d instead of zero\n", res);
	}
	else
	{
		DWORD err = GetLastError();
		if (err != ERROR_INVALID_PARAMETER)
		{
			failure("WideCharToMultiByte with input==output returned error %ld instead of ERROR_INVALID_PARAMETER\n", err);
		}
	}
	// MSDN: "ERROR_INSUFFICIENT_BUFFER. A supplied buffer size was not large enough, or it was incorrectly set to NULL."
	// The part "incorrectly set to NULL" is a lie!
	res = WideCharToMultiByte(CP_UTF8, 0, ab, 2, nullptr, sizeof(buffer), nullptr, nullptr);
	if (res != 0)
	{
		failure("WideCharToMultiByte with null output returned %d instead of zero\n", res);
	}
	else
	{
		DWORD err = GetLastError();
		if (err != ERROR_INVALID_PARAMETER)
		{
			failure("WideCharToMultiByte with null output returned error %ld instead of ERROR_INSUFFICIENT_BUFFER\n", err);
		}
	}

	// Test to code page different from UTF-8
	memset(buffer, 0x11, sizeof(buffer));
	res = WideCharToMultiByte(1251, 0, ab, -1, reinterpret_cast<char*>(buffer), sizeof(buffer), nullptr, nullptr);
	if (res == 0)
	{
		failure("Zero-terminated WideCharToMultiByte(1251) returned error %ld\n", GetLastError());
		return;
	}
	if (res != 3)
	{
		failure("Zero-terminated WideCharToMultiByte(1251) returned %d instead of 3\n", res);
	}
	if (buffer[0] != 0xe0 || buffer[1] != 0xe1 ||
		buffer[2] != 0x00 || buffer[3] != 0x11)
	{
		failure("Zero-terminated WideCharToMultiByte result is %x%x%x%x instead of e0e10011\n",
				(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2], (unsigned char)buffer[3]);
	}
	memset(buffer, 0x11, sizeof(buffer));
	// This call should success but produce substitute symbols
	res = WideCharToMultiByte(437, 0, ab, -1, reinterpret_cast<char*>(buffer), sizeof(buffer), nullptr, nullptr);
	if (res == 0)
	{
		failure("Zero-terminated WideCharToMultiByte(437) returned error %ld\n", GetLastError());
		return;
	}
	if (res != 3)
	{
		failure("Zero-terminated WideCharToMultiByte(437) returned %d instead of 3\n", res);
	}
	if (buffer[0] != 0x3f || buffer[1] != 0x3f ||
		buffer[2] != 0x00 || buffer[3] != 0x11)
	{
		failure("Zero-terminated WideCharToMultiByte result is %02x%02x%02x%02x instead of 3f3f0011\n",
				(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2], (unsigned char)buffer[3]);
	}

}
