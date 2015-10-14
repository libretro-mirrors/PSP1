#include "base/backtrace.h"

#if defined(__GLIBC__) && !defined(__UCLIBC__)
#include <execinfo.h>
#include <unistd.h>

static void *backtrace_buffer[128];

void PrintBacktraceToStderr() {
	int num_addrs = backtrace(backtrace_buffer, 128);
	backtrace_symbols_fd(backtrace_buffer, num_addrs, STDERR_FILENO);
}

#else

#include <stdio.h>

void PrintBacktraceToStderr() {
	fprintf(stderr, "No backtrace available to print on this platform\n");
}

#endif
