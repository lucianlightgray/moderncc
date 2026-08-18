/* T-mac-30118: the address of a __thread variable is TP-relative (runtime-only),
 * so it is NOT a compile-time constant and must be rejected as a static
 * initializer — pre-fix mcc emitted a plain absolute reloc giving the wrong
 * (link-time) pointer. gcc/clang both reject. */
__thread int t = 77;
int *p = &t;
