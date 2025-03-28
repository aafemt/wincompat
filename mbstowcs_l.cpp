/*************************************************
*
*  Implementation of _mbstowcs_l() function
*
**************************************************/

#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <stdlib.h>
#include <string.h>

size_t _mbstowcs_l(wchar_t* wcstr, const char* mbstr, size_t n, locale_t locale)
{
	if (mbstr == nullptr)
	{
		errno = EINVAL;
		return (size_t)-1;
	}

	size_t inLength = strlen(mbstr);
	if (inLength == 0)
	{
		if (wcstr != nullptr)
		{
			*wcstr = L'\0';
		}
		return 0;
	}

	const char* charset = nl_langinfo_l(CODESET, locale);
	iconv_t i = iconv_open("WCHAR_T", charset);
	if (i == (iconv_t)-1)
	{
		return (size_t)-1;
	}

	size_t result = 0;
	char* source = const_cast<char*>(mbstr);

	if (wcstr == nullptr)
	{
		wchar_t buf[64];
		while (inLength > 0)
		{
			char* dest = reinterpret_cast<char*>(buf);
			size_t outLength = sizeof(buf);

			size_t res = iconv(i, &source, &inLength, &dest, &outLength);
			if (res == (size_t)-1)
			{
				// String cannot contain incomplete sequence so EINVAL is also a fatal error
				if (errno != E2BIG)
				{
					result = (size_t)-1;
					break;
				}
			}
			result += (dest - reinterpret_cast<char*>(buf)) / sizeof(wchar_t);
		}
	}
	else
	{
		size_t outLength = n * sizeof(wchar_t);
		char* dest = reinterpret_cast<char*>(wcstr);

		size_t res = iconv(i, &source, &inLength, &dest, &outLength);
		if (res == (size_t)-1)
		{
			// String must flawlessly fit to output buffer, so any error is fatal.
			result = (size_t)-1;
		}
		else
		{
			result = (dest - reinterpret_cast<char*>(wcstr)) / sizeof(wchar_t);
			if (result < n) // otherwise buffer overflow will happen
			{
				wcstr[result] = L'\0';
			}
		}
	}
	iconv_close(i);

	return result;
}
