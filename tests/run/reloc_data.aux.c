#include <stdint.h>

int32_t aux_table[8] = {1, 2, 3, 4, 5, 6, 7, 8};
const char *aux_names[3] = {"alpha", "beta", "gamma"};
int32_t *aux_ptr = &aux_table[3];

static int32_t aux_priv[4] = {100, 200, 300, 400};
int32_t *aux_priv_ptr = &aux_priv[1];

const char aux_chars[6] = {'m', 'c', 'c', '-', 'r', 0};
