/* C99 6.4.4.1: integer constants -- bases, suffixes, and the type each constant
   takes (observable via sizeof and signed/unsigned comparison rules). All three
   compilers share this host's LP64 ABI, so the widths agree. */
#include <stdio.h>

int main(void)
{
    /* bases: decimal, octal (0...), hexadecimal (0x...) */
    printf("bases: %d %d %d\n", 255, 0377, 0xff);
    printf("octal0: %d\n", 0);

    /* suffix-driven width: int=4, long/long long per ABI */
    printf("widths: %d %d %d %d %d\n",
           (int)sizeof(1), (int)sizeof(1U), (int)sizeof(1L),
           (int)sizeof(1UL), (int)sizeof(1LL));

    /* constant typing (C99 6.4.4.1 table): a decimal constant that overflows
       int becomes long (never unsigned); a hex/octal constant may become
       unsigned. Observable through sizeof and signedness. */
    printf("dec_overflow_size: %d\n", (int)sizeof(2147483648));   /* -> long */
    printf("hex_unsigned_size: %d\n", (int)sizeof(0x80000000));   /* -> unsigned int */
    printf("hex_big_size: %d\n", (int)sizeof(0xFFFFFFFFFFFFFFFF));/* -> u long long */

    /* 0x80000000 is unsigned, so it is > 0; the signed comparison rule makes
       (-1 < 1U) false because -1 converts to a huge unsigned value */
    printf("signedness: %d %d %d\n",
           (0x80000000 > 0), (-1 < 1U), (-1 < 1));

    /* unsigned arithmetic wraps modulo 2^N (well defined) */
    printf("wrap: %u %u\n", 0u - 1u, 0xFFFFFFFFu + 1u);

    /* a hex constant with explicit suffix keeps its width */
    printf("suffixed: %llu\n", 0x100000000ULL);
    return 0;
}
