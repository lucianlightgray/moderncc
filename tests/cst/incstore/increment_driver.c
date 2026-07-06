/* Recursive dynamic-value re-include (docs/CST.md D3): the same header pulled in
 * repeatedly with an incrementing macro, depth-gated. */
#define ICAT(a, b) ICAT_(a, b)
#define ICAT_(a, b) a##b
#define INCREMENTING 1
#include "increment.h"
int increment_sum(void) { return level_1 + level_2; } /* 1 + 2 == 3 */
