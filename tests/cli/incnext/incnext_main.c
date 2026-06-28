#include <stdio.h>
#include "feature.h"            /* found in d1; it #include_next's d2's copy */
int main(void){ printf("%d %d\n", FEATURE_LAYER, FEATURE_BASE); return 0; }
