#define ICAT(a, b) ICAT_(a, b)
#define ICAT_(a, b) a##b
#define INCREMENTING 1
#include "increment.h"
int increment_sum(void) {
	return level_1 + level_2;
}
