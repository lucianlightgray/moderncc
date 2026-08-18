/* T-mac-30150: typeof_unqual works in C23 (strips const → writable). rc 0. */
int main(void){ const int x = 1; typeof_unqual(x) y; y = 3; return y - 3; }
