/* T-mac-30146: a surrogate UCN in a wide string is invalid in every mode and
 * must be rejected (gcc/clang both error). Pre-fix mcc accepted it under C23
 * and emitted the lone surrogate. Compiled -std=c23. */
int main(void){ unsigned short w[] = u"\uD800X"; return w[0]; }
