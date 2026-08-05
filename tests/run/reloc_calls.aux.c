#include <stdint.h>

int32_t aux_counter = 7;

static int32_t aux_hidden(int32_t x) { return x * 3 + 1; }

int32_t aux_add(int32_t a, int32_t b) { return a + b; }

int32_t aux_mul(int32_t a, int32_t b) { return a * b; }

int32_t aux_via_static(int32_t x) { return aux_hidden(x); }

extern int32_t main_shared;

int32_t aux_read_main(void) { return main_shared; }

extern int32_t main_callback(int32_t x);

int32_t aux_call_back(int32_t x) { return main_callback(x) + 1; }
