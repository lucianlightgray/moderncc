#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/string/strlen.c,v 1.7 2009/01/26 07:31:28 delphij Exp $");

#include <limits.h>
#include <sys/types.h>
#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_STRLEN

#if LONG_BIT == 32
static const unsigned long mask01 = 0x01010101;
static const unsigned long mask80 = 0x80808080;
#elif LONG_BIT == 64
static const unsigned long mask01 = 0x0101010101010101;
static const unsigned long mask80 = 0x8080808080808080;
#else
#error Unsupported word size
#endif

#define LONGPTR_MASK (sizeof(long) - 1)

#define testbyte(x)               \
	do {                          \
		if (p[x] == '\0')         \
			return (p - str + x); \
	} while (0)

size_t
_platform_strlen(const char *str) {
	const char *p;
	const unsigned long *lp;

	for (p = str; (uintptr_t)p & LONGPTR_MASK; p++)
		if (*p == '\0')
			return (p - str);

	for (lp = (const unsigned long *)p;; lp++)
		if ((*lp - mask01) & mask80) {
			p = (const char *)(lp);
			testbyte(0);
			testbyte(1);
			testbyte(2);
			testbyte(3);
#if (LONG_BIT >= 64)
			testbyte(4);
			testbyte(5);
			testbyte(6);
			testbyte(7);
#endif
		}

	return (0);
}

#if VARIANT_STATIC
size_t
strlen(const char *str) {
	return _platform_strlen(str);
}
#endif

#endif
