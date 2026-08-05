#include <stdint.h>

__thread int32_t aux_t_init = 11;

int32_t aux_t_get(void) { return aux_t_init; }

void aux_t_set(int32_t v) { aux_t_init = v; }
