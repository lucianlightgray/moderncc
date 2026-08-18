/* T-mac-30135 (slice): __attribute__((weakref("target"))) to a DEFINED target
 * now links and calls the target (previously emitted a strong undef to the
 * alias name → "unresolved reference"). Treated as a weak alias. Residual: an
 * UNDEFINED weakref target still errors (gcc resolves to 0). */
#include <stdio.h>
static int g(void) { return 42; }
static int wf(void) __attribute__((weakref("g")));
int main(void) { printf("weakref=%d\n", wf()); return wf() == 42 ? 0 : 1; }
