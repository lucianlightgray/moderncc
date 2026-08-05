#include <stdio.h>

extern inline int redef_plain(int a) { return a; }

int call_plain_early(void) { return redef_plain(10); }

int redef_plain(int b) { return 1 + b; }

int call_plain_late(void) { return redef_plain(20); }

extern inline int redef_inline(void) { return 0; }

inline int redef_inline(void) { return 1; }

extern int inline redef_static(void) { return 0; }

static int inline redef_static(void) { return 2; }

int main(void)
{
	int ok = 1;

	if (call_plain_early() != 11)
		ok = 0;
	if (call_plain_late() != 21)
		ok = 0;
	if (redef_plain(0) != 1)
		ok = 0;
	if (redef_inline() != 1)
		ok = 0;
	if (redef_static() != 2)
		ok = 0;
#ifndef __GNUC_GNU_INLINE__
	ok = 0;
#endif
	printf(ok ? "OK\n" : "FAIL\n");
	return 0;
}
