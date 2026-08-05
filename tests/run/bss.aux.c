#include <stdint.h>

int32_t aux_big[131072];
char aux_pad[65536];

int32_t aux_probe(int32_t i) { return aux_big[i]; }
