#pragma once

#include "windows.h"

typedef unsigned WINAPI ThreadFunc(void*);
#define THREAD_FUNC_IMPL(name) unsigned WINAPI name(void* param)

HANDLE _beginthreadex(void*, unsigned stack_size, ThreadFunc* start_address, void* arglist, unsigned initflags, unsigned* tid);
