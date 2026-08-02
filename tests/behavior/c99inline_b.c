#include "c99inline.h"



int (*c99inline_pb)(int, int, int) = c99inline_add3;

int c99inline_use_b(void) { return c99inline_pb(5, 6, 6); }

void *c99inline_b_addr(void) { return (void *)c99inline_pb; }
