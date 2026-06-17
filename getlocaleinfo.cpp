/*************************************************************
*
*  Implementation of GetLocaleInfo function
*
**************************************************************/

#include <langinfo.h>
#include <locale.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <unordered_map>

#include "winnls.h"
#include "winerror.h"

namespace
{

	// Cache for locales received from newlocale
	struct LocaleCache
	{
		pthread_mutex_t listGuard = PTHREAD_MUTEX_INITIALIZER;
		std::unordered_map<std::string, locale_t> list;

		~LocaleCache();

		const std::string getLocaleName(LCID loc);
		locale_t get(LCID locale);
	};

	LocaleCache::~LocaleCache()
	{
		// For applications it is ok to leave unfreed resources but libraries can be unloaded earlier
		for (auto itr : list)
		{
			if (itr.second != (locale_t)0)
			{
				freelocale(itr.second);
			}
		}
		pthread_mutex_destroy(&listGuard);
	}

	const std::string LocaleCache::getLocaleName(LCID loc)
	{
		switch (loc)
		{
		case LOCALE_INVARIANT:
			return "POSIX";
		case LOCALE_USER_DEFAULT:
			return "";
		default:
			return "SurelyNonExistingLocale";
		}
	}

	locale_t LocaleCache::get(LCID locale)
	{
		pthread_mutex_lock(&listGuard);
		const std::string localeName = getLocaleName(locale);
		auto itr = list.emplace(localeName, (locale_t)0);
		if (itr.second)
		{
			// NEGATIVE_SIGN and POSITIVE_SIGN are in LC_MONETARY
			itr.first->second = newlocale(LC_TIME_MASK | LC_NUMERIC_MASK | LC_MONETARY_MASK, localeName.c_str(), (locale_t)0);
		}
		locale_t result = itr.first->second; // Better to have it inside of lock to avoid iterator invalidation
		pthread_mutex_unlock(&listGuard);
		return result;
	}

	LocaleCache cache;

}

int GetLocaleInfo(LCID locale, LCTYPE LCType, wchar_t* lpLCData, int cchData)
{
	locale_t loc = cache.get(locale);
	if (loc == (locale_t)0)
	{
		if (errno == ENOENT)
		{
			errno = ERROR_INVALID_PARAMETER; // That's what Windows return for wrong locale
		}
		return 0;
	}
	const char* value = nl_langinfo_l(LCType, loc);
	size_t valueLength = strlen(value);

	if (valueLength != 0)
	{
		++valueLength;
		size_t r = _mbstowcs_l(lpLCData, value, cchData, loc);
		if (r == (size_t)-1)
		{
			return 0;
		}
		if (cchData == 0)
		{
			// "If the function succeeds anf the value of cchData is 0, the return value
			// is the required size, in characters including a null character,
			// for the locale data buffer.
			return r + 1;
		}
		if (r >= (size_t)cchData) // GetLocaleInfo always terminate data.
		{
			errno = ERROR_INSUFFICIENT_BUFFER;
			return 0;
		}
		return r + 1;
	}

	// Here either value is really an empty string or the item is invalid.
	// We have no way (?) to tell which is true.
	if (cchData != 0)
		*lpLCData = L'\0';

	return 1;
}

