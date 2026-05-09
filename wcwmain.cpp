/************** Emulation of wmain() entry point *******************/
#include <locale.h>
#include <memory>
#include <stdlib.h>

// All possible variants, including undecorated one.
// "Weak" attribute helps to avoid "undefined reference" error with --no-undefined linker option.
int __attribute__((weak)) wmain(int argc, wchar_t* argv[], wchar_t* env[]);
int __attribute__((weak)) wmain(int argc, wchar_t* argv[]);
int __attribute__((weak)) wmain(int argc);
int __attribute__((weak)) wmain();
// This is dangerous because of lack of compile-time parameter type matching
extern "C" int __attribute__((weak)) wmain(...);

void paramsToUnicode(char* params[], std::unique_ptr<wchar_t*[]>& pointers, std::unique_ptr<wchar_t[]>& data)
{
	int n = 0;
	size_t size = 0;
	// Calculate number of params and size of data
	for (char** itr = params; *itr; ++itr, ++n)
	{
		size_t paramSize = mbstowcs(nullptr, *itr, 0) + 1;
		if (paramSize == 0) // (size_t)-1 + 1 is actually an undefined behavior
		{
			// What to do here???
			abort();
		}
		size += paramSize;
	}
	data.reset(new wchar_t[size]);
	pointers.reset(new wchar_t*[n + 1]);
	wchar_t* p = data.get();
	for (int i = 0; i < n; ++i)
	{
		pointers[i] = p;
		// Errors would appear earlier
		p += mbstowcs(p, params[i], size);
		*p++ = L'\0';
	}
	pointers[n] = nullptr;
}

// This should make this function "a fallback" for the case if application
// has own main()
int __attribute__((weak)) main(int argc, char* argv[], char* env[])
{
	// Set locale for convenience and make sure that mbstowcs works right
	setlocale(LC_ALL, "");

	if (static_cast<int(*)(int,wchar_t**,wchar_t**)>(wmain))
	{
		std::unique_ptr<wchar_t[]> argvData;
		std::unique_ptr<wchar_t*[]> argvPointers;
		paramsToUnicode(argv, argvPointers, argvData);
		std::unique_ptr<wchar_t[]> envData;
		std::unique_ptr<wchar_t*[]> envPointers;
		paramsToUnicode(env, envPointers, envData);
		static_cast<int(*)(int,wchar_t**,wchar_t**)>(wmain)(argc, argvPointers.get(), envPointers.get());
	}
	else if (static_cast<int(*)(int,wchar_t**)>(wmain))
	{
		std::unique_ptr<wchar_t[]> argvData;
		std::unique_ptr<wchar_t*[]> argvPointers;
		paramsToUnicode(argv, argvPointers, argvData);
		static_cast<int(*)(int,wchar_t**)>(wmain)(argc, argvPointers.get());
	}
	else if (static_cast<int(*)(int)>(wmain))
	{
		static_cast<int(*)(int)>(wmain)(argc);
	}
	else if (static_cast<int(*)()>(wmain))
	{
		static_cast<int(*)()>(wmain)();
	}
	else if (static_cast<int(*)(...)>(wmain))
	{
		std::unique_ptr<wchar_t[]> argvData;
		std::unique_ptr<wchar_t*[]> argvPointers;
		paramsToUnicode(argv, argvPointers, argvData);
		std::unique_ptr<wchar_t[]> envData;
		std::unique_ptr<wchar_t*[]> envPointers;
		paramsToUnicode(env, envPointers, envData);
		static_cast<int(*)(...)>(wmain)(argc, argvPointers.get(), envPointers.get());
	}
	else
		abort();
}
