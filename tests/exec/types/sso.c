/* C reverse scalar_storage_order (T-lin-10010, slice 1): integer scalar members
 * of a big-endian struct are stored byte-reversed on this little-endian target,
 * verified byte-for-byte equal to gcc-16.  &member and non-scalar members are
 * refused (see tests/diagnostics/dg-error/scalar_storage_order_be.c). */
#include <stdio.h>
#include <string.h>

struct __attribute__((scalar_storage_order("big-endian"))) S {
	unsigned int u;
	int i;
	short s;
	unsigned long long ll;
	unsigned char c;
};

int main(void) {
	struct S x;
	memset(&x, 0, sizeof x);
	x.u = 0x11223344u;
	x.i = -2;
	x.s = 0x0102;
	x.ll = 0x0102030405060708ull;
	x.c = 0xAB;

	unsigned char b[sizeof x];
	memcpy(b, &x, sizeof x);

	int ok =
			/* each member is laid out big-endian */
			b[0] == 0x11 && b[1] == 0x22 && b[2] == 0x33 && b[3] == 0x44 &&
			b[4] == 0xff && b[5] == 0xff && b[6] == 0xff && b[7] == 0xfe &&
			b[8] == 0x01 && b[9] == 0x02 &&
			b[16] == 0x01 && b[17] == 0x02 && b[22] == 0x07 && b[23] == 0x08 &&
			b[24] == 0xAB &&
			/* reads swap back to the stored value */
			x.u == 0x11223344u && x.i == -2 && (unsigned short)x.s == 0x0102 &&
			x.ll == 0x0102030405060708ull && x.c == 0xAB;

	/* arithmetic round-trips through the load-swap and store-swap */
	x.u++;
	x.i -= 100;
	x.ll *= 2;
	memcpy(b, &x, sizeof x);
	ok = ok && x.u == 0x11223345u && x.i == -102 &&
			x.ll == 0x020406080a0c0e10ull && b[0] == 0x11 && b[3] == 0x45 &&
			b[16] == 0x02 && b[23] == 0x10;

	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
