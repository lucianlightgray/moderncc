#include <stdint.h>

__thread int32_t aux_tls_init = 7;
__thread int32_t aux_tls_zero;

static __thread int32_t aux_tls_priv = 3;

int32_t aux_tls_get(void) { return aux_tls_init; }

void aux_tls_set(int32_t v) { aux_tls_init = v; }

int32_t aux_tls_zero_get(void) { return aux_tls_zero; }

int32_t aux_tls_priv_bump(void) { return ++aux_tls_priv; }
