#pragma once

#include <string.h>

#include "base/arch.h"
#include "base/backtrace.h"
#include "base/compat.h"

// Simple wrapper around Android's logging interface that also allows other
// implementations, and also some misc utilities.

#ifdef _MSC_VER
// Disable annoying warnings in VS
#pragma warning (disable:4996)   //strcpy may be dangerous, etc.
#endif

#undef Crash

#include <stdio.h>
// Logging
#ifdef _WIN32

#ifdef _M_X64
inline void Crash() { int *x = (int *)1337; *x = 1; }
#else
inline void Crash() { __asm { int 3 }; }
#endif

#else

#if defined(_M_IX86) || defined(_M_X64)
inline void Crash() {
	PrintBacktraceToStderr();
	asm("int $0x3");
}
#else
inline void Crash() {
	PrintBacktraceToStderr();
	char *p = (char *)1337;
	*p = 1;
}
#endif

#endif

// Just ILOGs on nonWindows. On Windows it outputs to the VS output console.
void OutputDebugStringUTF8(const char *p);

#ifdef _WIN32

#define XLOG_IMPL(type, ...) do {\
	char temp[512]; \
	char *p = temp; \
	int len = snprintf(p, sizeof(temp), type ": %s:%i: ", __FILE__, __LINE__); \
	if (len < sizeof(temp)) { \
		p += len; \
		p += snprintf(p, sizeof(temp) - len - 3, type ": " __VA_ARGS__);  \
		if (p > temp + sizeof(temp) - 3) \
			p = temp + sizeof(temp) - 3; \
		p += sprintf(p, "\n"); \
		OutputDebugStringUTF8(temp); \
	} \
} while (false)

#ifdef _DEBUG
#define DLOG(...) XLOG_IMPL("D", __VA_ARGS__)
#else
#define DLOG(...)
#endif

#define ILOG(...) XLOG_IMPL("I", __VA_ARGS__)
#define WLOG(...) XLOG_IMPL("W", __VA_ARGS__)
#define ELOG(...) XLOG_IMPL("E", __VA_ARGS__)
#define FLOG(...) do {XLOG_IMPL("F", __VA_ARGS__); Crash();} while (false)

// TODO: Win32 version using OutputDebugString
#else

#include <stdio.h>

inline const char *removePath(const char *str) {
	const char *slash = strrchr(str, '/');
	return slash ? (slash + 1) : str;
}

#ifdef _DEBUG
#define DLOG(...) {printf("D: %s:%i: ", removePath(__FILE__), __LINE__); printf("D: " __VA_ARGS__); printf("\n");}
#else
#define DLOG(...)
#endif
#define ILOG(...) {printf("I: %s:%i: ", removePath(__FILE__), __LINE__); printf("I: " __VA_ARGS__); printf("\n");}
#define WLOG(...) {printf("W: %s:%i: ", removePath(__FILE__), __LINE__); printf("W: " __VA_ARGS__); printf("\n");}
#define ELOG(...) {printf("E: %s:%i: ", removePath(__FILE__), __LINE__); printf("E: " __VA_ARGS__); printf("\n");}
#define FLOG(...) {printf("F: %s:%i: ", removePath(__FILE__), __LINE__); printf("F: " __VA_ARGS__); printf("\n"); Crash();}

#endif

#undef CHECK

#define CHECK(a) {if (!(a)) {FLOG("%i: CHECK failed on this line", __LINE__);}}
#define CHECK_P(a, ...) {if (!(a)) {FLOG("CHECK failed: " __VA_ARGS__);}}
#define CHECK_EQ(a, b) CHECK((a) == (b));
#define CHECK_NE(a, b) CHECK((a) != (b));
#define CHECK_GT(a, b) CHECK((a) > (b));
#define CHECK_GE(a, b) CHECK((a) >= (b));
#define CHECK_LT(a, b) CHECK((a) < (b));
#define CHECK_LE(a, b) CHECK((a) <= (b));
