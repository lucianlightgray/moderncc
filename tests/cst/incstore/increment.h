#if (INCREMENTING) < 3
int ICAT(level_, INCREMENTING) = INCREMENTING;

#if (INCREMENTING) == 1
#undef INCREMENTING
#define INCREMENTING 2
#elif (INCREMENTING) == 2
#undef INCREMENTING
#define INCREMENTING 3
#endif

#include "increment.h"
#endif
