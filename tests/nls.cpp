/*************************************************
*
*   Test for NLS functions
*
**************************************************/

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
		failure("Got value %S while expected %S\n", buffer, expected);
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
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SPOSITIVESIGN, L"+");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SNEGATIVESIGN, L"-");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_SDECIMAL, L".");
	checkLocaleInfo(LOCALE_INVARIANT, LOCALE_STHOUSAND, L",");
	checkLocaleInfo(0x1111, LOCALE_SABBREVDAYNAME7, L"NO LOCALE", ERROR_INVALID_PARAMETER);
}
