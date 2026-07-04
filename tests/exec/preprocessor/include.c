#include <stdio.h>

int main() {
	printf("including\n");
#include "include.h"
#define test_missing_nl
	printf("done\n");

#define INC "include.h"

#ifdef __has_include
#if defined __has_include
#if __has_include("include.h")
	printf("has_include\n");
#endif
#if __has_include(INC)
	printf("has_include\n");
#endif
#if __has_include("not_found_18_include.h")
	printf("has_include not found\n");
#endif
#endif
#endif

#ifdef __has_include_next
#if defined __has_include_next
#if __has_include_next("include.h")
	printf("has_include_next\n");
#endif
#if __has_include_next(INC)
	printf("has_include_next\n");
#endif
#if __has_include_next("not_found_18_include.h")
	printf("has_include_next not found\n");
#endif
#endif
#endif

#include "include2.h"
#include "./include2.h"
#include "../preprocessor/include2.h"

	return 0;
}
