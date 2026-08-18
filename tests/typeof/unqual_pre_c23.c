/* T-mac-30150: typeof_unqual is C23-only, NOT a GNU extension — in a pre-C23
 * GNU dialect it must be an ordinary identifier, so this use as a keyword must
 * fail to compile (both gcc and clang reject). Compiled -std=gnu17. */
int main(void){ const int x = 1; typeof_unqual(x) y; y = 3; return y - 3; }
