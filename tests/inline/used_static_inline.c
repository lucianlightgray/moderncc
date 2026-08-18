/* T-mac-30114: an unreferenced `static inline __attribute__((used))` function
 * must be emitted (used forces it), while a plain unreferenced static inline is
 * not. keepme must appear in the object symbol table; dropme must not. */
static inline __attribute__((used)) int keepme(int x) { return x + 1; }
static inline int dropme(int x) { return x + 2; }
int anchor(void) { return 0; }
