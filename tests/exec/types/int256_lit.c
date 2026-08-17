/* T-lin-10013: the i256 / ui256 literal suffix (MSVC i64-family spelling) for
 * __int256.  A >64-bit constant written directly must be byte-identical to the
 * shifted/or-ed composition it replaces, in every base, both signednesses,
 * with the u on either side. */
#if defined test_lit

#include <stdio.h>
#include <string.h>
int main(void) {
	__int256 a = 0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20i256;
	__int256 b = (((__int256)0x0102030405060708ull << 192) |
								((__int256)0x090a0b0c0d0e0f10ull << 128) |
								((__int256)0x1112131415161718ull << 64) |
								(__int256)0x191a1b1c1d1e1f20ull);
	__int256 d = 340282366920938463463374607431768211456i256; /* 2^128 */
	__int256 e = (__int256)1 << 128;
	unsigned char ba[32], bb[32];
	int i;
	memcpy(ba, &a, 32);
	memcpy(bb, &b, 32);
	printf("hexeq %d\n", memcmp(ba, bb, 32) == 0);
	printf("hex ");
	for (i = 0; i < 32; i++)
		printf("%02x", ba[i]);
	printf("\n");
	memcpy(ba, &d, 32);
	memcpy(bb, &e, 32);
	printf("deceq %d\n", memcmp(ba, bb, 32) == 0);
	printf("dec ");
	for (i = 0; i < 32; i++)
		printf("%02x", ba[i]);
	printf("\n");
	return 0;
}

#elif defined test_forms

#include <stdio.h>
int main(void) {
	printf("zero %lld\n", (long long)0i256);
	printf("oct %lld\n", (long long)0777i256);
	printf("bin %lld\n", (long long)0b101011i256);
	printf("upre %llu\n", (unsigned long long)0xffui256);
	printf("upost %llu\n", (unsigned long long)0xACE0i256u);
	printf("sub1 %lld\n", (long long)(0i256 - 1i256));
	printf("size %d %d\n", (int)sizeof(0i256), (int)sizeof(0ui256));
	return 0;
}

#elif defined test_generic

#include <stdio.h>
int main(void) {
	printf("g %d %d %d\n",
				 _Generic(0i256, __int256: 1, unsigned __int256: 2, default: 0),
				 _Generic(0ui256, __int256: 1, unsigned __int256: 2, default: 0),
				 _Generic(1i256 + 1i256, __int256: 1, default: 0));
	return 0;
}

#endif
