#include "c99inline.h"

/* Second TU: also takes the address of the plain-inline function. Its &add3 must
   resolve to the SAME weak out-of-line copy as the first TU's. */
int (*c99inline_pb)(int, int, int) = c99inline_add3;

int c99inline_use_b(void) { return c99inline_pb(5, 6, 6); } /* 17 */

void *c99inline_b_addr(void) { return (void *)c99inline_pb; }
