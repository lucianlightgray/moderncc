#include <stddef.h>
#define WIDGET_ENABLED 1

#if WIDGET_ENABLED
int widget = 1;
#else
int widget = 0;
#endif

#ifndef WIDGET_GUARD
#define WIDGET_GUARD
#endif

int pp_kinds(void) {
	return widget + WIDGET_ENABLED;
}
