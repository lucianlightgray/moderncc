/* D1c — PP-concrete nodes (docs/CST.md §D1c): IncludeDirective ("Include"),
 * PPDirective, PPConditional ("PPCond"). The directive lines never reach the
 * post-expansion parser; they are captured at the preprocessor boundary. */
#include <stddef.h>
#define WIDGET_ENABLED 1

#if WIDGET_ENABLED
int widget = 1;
#else
int widget = 0;
#endif

#ifndef WIDGET_GUARD
#  define WIDGET_GUARD
#endif

int pp_kinds(void) {
	return widget + WIDGET_ENABLED;
}
