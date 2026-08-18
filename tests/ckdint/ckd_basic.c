/* T-mac-30156: <stdckdint.h> must work on every target mcc supports (the
 * header must not require __int128). Exit 0 iff every ckd_* result + overflow
 * flag matches the C23-defined semantics (== the __builtin_*_overflow result). */
#include <stdckdint.h>
int main(void){
    int r; _Bool o;
    o = ckd_add(&r, 2000000000, 2000000000); if (!o || r != -294967296) return 1;
    o = ckd_sub(&r, -2000000000, 2000000000); if (!o || r != 294967296) return 2;
    o = ckd_mul(&r, 100000, 100000); if (!o || r != 1410065408) return 3;
    o = ckd_add(&r, 2, 3); if (o || r != 5) return 4;               /* no overflow */
    unsigned char uc; o = ckd_add(&uc, 200, 100); if (!o || uc != 44) return 5; /* narrow result */
    unsigned int u; o = ckd_mul(&u, 3000000000u, 2u); if (!o || u != 1705032704u) return 6;
    long long ll; o = ckd_mul(&ll, 3000000000LL, 4000000000LL); if (!o) return 7; /* fits in i64? 1.2e19>9.2e18 → ovf */
#if __STDC_VERSION_STDCKDINT_H__ != 202311L
    return 8;
#endif
    return 0;
}
