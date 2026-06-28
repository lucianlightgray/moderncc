/* __attribute__((__mode__(...))) rewrites an integer declaration's width.
   The mode attribute applies to the base type, so it sits between the type
   keyword and the declared name; the GCC token spellings are __QI__ etc. */
#include <stdio.h>

int __attribute__((__mode__(__QI__))) qv;   /* 1 byte  */
int __attribute__((__mode__(__HI__))) hv;   /* 2 bytes */
int __attribute__((__mode__(__SI__))) sv;   /* 4 bytes */
int __attribute__((__mode__(__DI__))) dv;   /* 8 bytes */

int main(void)
{
    printf("%zu %zu %zu %zu\n",
           sizeof(qv), sizeof(hv), sizeof(sv), sizeof(dv));
    dv = 1;
    dv <<= 40;            /* well-defined only because mode(DI) made it 8 bytes */
    printf("%lld\n", (long long)dv);
    return 0;
}
