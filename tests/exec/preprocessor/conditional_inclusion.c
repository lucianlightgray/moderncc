#include <stdio.h>

#define LEVEL 3
#define FEATURE_A 1

int main(void) {

#if 2 + 3 * 4 == 14
	printf("arith: yes\n");
#else
	printf("arith: no\n");
#endif

#if LEVEL > 2 && LEVEL <= 5
	printf("level: %d in range\n", LEVEL);
#endif

#if defined(FEATURE_A) && !defined(FEATURE_B)
	printf("features: A only\n");
#endif

#if UNDEFINED_THING
	printf("undef: nonzero\n");
#else
	printf("undef: zero\n");
#endif

#if LEVEL == 1
	printf("branch: one\n");
#elif LEVEL == 2
	printf("branch: two\n");
#elif LEVEL == 3
	printf("branch: three\n");
#else
	printf("branch: other\n");
#endif

#ifdef FEATURE_A
#ifndef FEATURE_B
	printf("nested: A set, B unset\n");
#endif
#endif
	return 0;
}
