#include <stdio.h>

int __attribute__((__mode__(__QI__))) qv;
int __attribute__((__mode__(__HI__))) hv;
int __attribute__((__mode__(__SI__))) sv;
int __attribute__((__mode__(__DI__))) dv;
float __attribute__((__mode__(__SF__))) fv;
double __attribute__((__mode__(__DF__))) gv;

int main(void) {
	printf("%zu %zu %zu %zu\n",
				 sizeof(qv), sizeof(hv), sizeof(sv), sizeof(dv));
	printf("%zu %zu\n", sizeof(fv), sizeof(gv));
	dv = 1;
	dv <<= 40;
	printf("%lld\n", (long long)dv);
	return 0;
}
