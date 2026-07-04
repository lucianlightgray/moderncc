#include <stdio.h>

int __attribute__((__mode__(__QI__))) qv;
int __attribute__((__mode__(__HI__))) hv;
int __attribute__((__mode__(__SI__))) sv;
int __attribute__((__mode__(__DI__))) dv;

int main(void) {
    printf("%zu %zu %zu %zu\n",
           sizeof(qv), sizeof(hv), sizeof(sv), sizeof(dv));
    dv = 1;
    dv <<= 40;
    printf("%lld\n", (long long)dv);
    return 0;
}
