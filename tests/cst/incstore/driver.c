/* D3 live capture: leaf.h is included via wrap.h and directly twice; all three
 * references must hash-cons to ONE SourceFile template, and both direct
 * IncludeDirective nodes must bind to it. */
#include "wrap.h"
#include "leaf.h"
#include "leaf.h"
int driver_main(void) {
	return leaf_val + wrap_val;
}
