/* T-mac-30170 / T-mac-30171: on macOS __builtin_mempcpy must link (memcpy +
 * return dst+n) and __builtin_bzero(p,n) must zero exactly n bytes (the old
 * macro dropped the size and zeroed 1). Exit 0 iff correct. */
int main(void){
    /* bzero: n bytes, not 1 */
    char z[8]; for (int i = 0; i < 8; i++) z[i] = (char)0xFF;
    __builtin_bzero(z, 8);
    for (int i = 0; i < 8; i++) if (z[i] != 0) return 1;
    __builtin_bzero(z + 0, 0);            /* n=0: no-op, must not crash */

    /* mempcpy: copies n bytes, returns dst+n */
    char d[8]; for (int i = 0; i < 8; i++) d[i] = '.';
    char *e = (char *)__builtin_mempcpy(d, "XYZ", 3);
    if (d[0] != 'X' || d[1] != 'Y' || d[2] != 'Z' || d[3] != '.') return 2;
    if (e != d + 3) return 3;

    /* _mempcpy_chk (FORTIFY) path must work too */
    char c[8]; for (int i = 0; i < 8; i++) c[i] = (char)0xFF;
    __builtin___mempcpy_chk(c, "AB", 2, sizeof c);
    if (c[0] != 'A' || c[1] != 'B') return 4;
    return 0;
}
