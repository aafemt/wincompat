#pragma once

#include <unistd.h>
// A handy class that can be useful in more than one module
struct FileHolder
{
	int h = -1;
	FileHolder(int handle): h(handle) {}
	~FileHolder()
	{
		if (h != -1)
		{
			close(h);
		}
	}
	int get()
	{
		return h;
	}
	int release()
	{
		int tmp = h;
		h = -1;
		return tmp;
	}
	explicit operator bool()
	{
		return h != -1;
	}
};

#define FileHolder(x) static_assert(false, "FileHolder cannot be temporary")
