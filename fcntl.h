#pragma once

#include_next <fcntl.h>

// This bit flag has no use on Linux, so define it to zero to make it no-op
const int O_BINARY = 0;

