/************************************************
*
*	Tests for string functions
*
*************************************************/

#include <errno.h>
#include <string.h>

#include "globals.h"

void RunStringTests()
{
	char buffer[10] = { "Start" };
	int res = strcat_s(buffer, sizeof(buffer), "End");
	if  (res != 0)
	{
		failure("strcat_s returned %d instead of zero\n", res);
	}
	if (strcmp(buffer, "StartEnd") != 0)
	{
		failure("strcat_s produced %s instead of \"StartEnd\"\n", buffer);
	}
	errno = 0;
	res = strcat_s(nullptr, sizeof(buffer), "End");
	if (res != EINVAL)
	{
		failure("strcat_s returned %d instead of EINVAL on null destination\n", res);
	}
	if (errno != EINVAL)
	{
		failure("strcat_s set errno to %d instead of EINVAL on null destination\n", res);
	}
	errno = 0;
	res = strcat_s(buffer, 2, "End");
	if (res != EINVAL)
	{
		failure("strcat_s returned %d instead of EINVAL on not terminated destination\n", res);
	}
	if (errno != EINVAL)
	{
		failure("strcat_s set errno to %d instead of EINVAL on not terminated destination\n", res);
	}
	if (buffer[0] != '\0')
	{
		failure("strcat_s dodn't set not terminated destination to empty string\n");
	}
	errno = 0;
	res = strcat_s(buffer, sizeof(buffer), nullptr);
	if (res != EINVAL)
	{
		failure("strcat_s returned %d instead of EINVAL on null source\n", res);
	}
	if (errno != EINVAL)
	{
		failure("strcat_s set errno to %d instead of EINVAL on null source\n", res);
	}
	if (buffer[0] != '\0')
	{
		failure("strcat_s didn't set destination to empty string on null source\n");
	}
	errno = 0;
	buffer[0] = 'S';
	res = strcat_s(buffer, 0, "End");
	if (res != EINVAL)
	{
		failure("strcat_s returned %d instead of EINVAL on zero destination\n", res);
	}
	if (errno != EINVAL)
	{
		failure("strcat_s set errno to %d instead of EINVAL on zero destination\n", res);
	}
	if (buffer[0] == '\0')
	{
		failure("strcat_s set destination to empty string on zero destination\n");
	}
	// Test priority of errors
	errno = 0;
	buffer[0] = 'S';
	res = strcat_s(buffer, 0, nullptr);
	if (res != EINVAL)
	{
		failure("strcat_s returned %d instead of EINVAL on zero destination and null source\n", res);
	}
	if (errno != EINVAL)
	{
		failure("strcat_s set errno to %d instead of EINVAL on zero destination and null source\n", res);
	}
	if (buffer[0] == '\0')
	{
		failure("strcat_s set destination to empty string on zero destination and null source\n");
	}

	errno = 0;
	strcpy(buffer, "Start");
	res = strcat_s(buffer, 3, nullptr);
	if (res != EINVAL)
	{
		failure("strcat_s returned %d instead of EINVAL on unterminated destination and null source\n", res);
	}
	if (errno != EINVAL)
	{
		failure("strcat_s set errno to %d instead of EINVAL on unterminated destination and null source\n", res);
	}
	if (buffer[0] != '\0')
	{
		failure("strcat_s didn't set destination to empty string on unterminated destination and null source\n");
	}

	errno = 0;
	strcpy(buffer, "Start");
	res = strcat_s(buffer, sizeof(buffer), "LongLongEnd");
	if (res != ERANGE)
	{
		failure("strcat_s returned %d instead of ERANGE on insufficiend destination\n", res);
	}
	if (errno != ERANGE)
	{
		failure("strcat_s set errno to %d instead of EINVAL on insufficient destination\n", res);
	}
	if (buffer[0] != '\0')
	{
		failure("strcat_s didn't set destination to empty string on insufficient destination\n");
	}

}
