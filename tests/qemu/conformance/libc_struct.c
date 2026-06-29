/* Self-checking struct-by-value ABI conformance, exercised against the target's
   real libc. div()/ldiv()/lldiv() *return a struct by value* (div_t {int,int},
   ldiv_t {long,long}, lldiv_t {long long,long long}) -- so the small-struct
   return ABI is checked against the platform libc, not just against mcc's own
   codegen. div_t (8 bytes) returns in a single GP register / register pair;
   lldiv_t (16 bytes on LP64) returns in two registers or via a hidden sret
   pointer depending on arch -- a classic place for a from-scratch compiler to
   diverge from the platform ABI. Endianness-independent; 0 on success. */

#include <stdlib.h>

int main(void)
{
    /* div_t: two ints packed into one 8-byte return slot */
    div_t d = div(17, 5);
    if (d.quot != 3 || d.rem != 2) return 1;

    /* negative dividend: truncation toward zero, sign of remainder follows
       dividend (C99 6.5.5p6) -- and confirms both fields decode correctly */
    div_t dn = div(-17, 5);
    if (dn.quot != -3 || dn.rem != -2) return 2;

    /* ldiv_t: two longs (8 bytes each on LP64 -> 16-byte aggregate) */
    ldiv_t l = ldiv(1000003L, 1000L);
    if (l.quot != 1000L || l.rem != 3L) return 3;

    /* lldiv_t: two long longs, value beyond 32 bits so a truncated field shows */
    lldiv_t ll = lldiv(0x100000007LL, 0x100000000LL);
    if (ll.quot != 1 || ll.rem != 7) return 4;

    lldiv_t ll2 = lldiv(10000000000LL, 7LL);
    if (ll2.quot != 1428571428LL || ll2.rem != 4LL) return 5;

    return 0;
}
