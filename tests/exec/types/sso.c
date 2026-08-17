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

	/* slice 2a: float/double members swap their bit-pattern */
	struct __attribute__((scalar_storage_order("big-endian"))) SF {
		float f;
		double d;
		unsigned int u;
	} y;
	memset(&y, 0, sizeof y);
	y.f = 1.5f;
	y.d = 3.14159265358979;
	y.u = 0xAABBCCDDu;
	unsigned char fb[sizeof y];
	memcpy(fb, &y, sizeof y);
	ok = ok &&
			/* 1.5f = 0x3fc00000 big-endian; the double is at offset 8 */
			fb[0] == 0x3f && fb[1] == 0xc0 && fb[2] == 0x00 && fb[3] == 0x00 &&
			fb[8] == 0x40 && fb[9] == 0x09 && fb[15] == 0x11 &&
			/* reads swap back, and float/double COMPARES are correct (the offset,
			 * >4-byte, compare-context class that slice 1 miscompiled on x86_64) */
			y.f == 1.5f && y.d == 3.14159265358979 && y.u == 0xAABBCCDDu;
	y.f = y.f * 2.0f;
	y.d = y.d + 1.0;
	ok = ok && y.f == 3.0f && y.d == 4.14159265358979;

	struct __attribute__((scalar_storage_order("big-endian"))) SA {
		int ia[3];
		float fa[2];
	} z;
	memset(&z, 0, sizeof z);
	z.ia[0] = 0x11223344;
	z.ia[1] = -2;
	z.ia[2] = 0x55667788;
	z.fa[0] = 1.5f;
	z.fa[1] = -2.25f;
	unsigned char ab[sizeof z];
	memcpy(ab, &z, sizeof z);
	ok = ok &&
			ab[0] == 0x11 && ab[3] == 0x44 &&
			ab[4] == 0xff && ab[7] == 0xfe &&
			ab[8] == 0x55 && ab[11] == 0x88 &&
			ab[12] == 0x3f && ab[13] == 0xc0 &&
			z.ia[0] == 0x11223344 && z.ia[1] == -2 && z.ia[2] == 0x55667788 &&
			z.fa[0] == 1.5f && z.fa[1] == -2.25f;
	for (int i = 0; i < 3; i++)
		z.ia[i] += 1;
	int k = 1;
	z.fa[k] = z.fa[k] * 2.0f;
	ok = ok && z.ia[0] == 0x11223345 && z.ia[1] == -1 &&
			z.ia[2] == 0x55667789 && z.fa[1] == -4.5f && z.fa[0] == 1.5f;

	struct SIP {
		int x;
		short y;
	};
	struct __attribute__((scalar_storage_order("big-endian"))) SIB {
		int x;
	};
	struct __attribute__((scalar_storage_order("big-endian"))) S2 {
		struct SIP plain;
		struct SIB be;
		struct SIP arr[2];
		int flat;
	} w;
	memset(&w, 0, sizeof w);
	w.plain.x = 0x11223344;
	w.be.x = 0x11223344;
	w.arr[1].x = 0x55667788;
	w.flat = 0x0a0b0c0d;
	unsigned char cb[sizeof w];
	memcpy(cb, &w, sizeof w);
	ok = ok && sizeof w == 32 &&
			cb[0] == 0x44 && cb[3] == 0x11 &&
			cb[8] == 0x11 && cb[11] == 0x44 &&
			cb[20] == 0x88 && cb[23] == 0x55 &&
			cb[28] == 0x0a && cb[31] == 0x0d &&
			w.plain.x == 0x11223344 && w.be.x == 0x11223344 &&
			w.arr[1].x == 0x55667788 && w.flat == 0x0a0b0c0d;
	w.flat += 1;
	w.be.x += 1;
	w.plain.x += 1;
	ok = ok && w.flat == 0x0a0b0c0e && w.be.x == 0x11223345 &&
			w.plain.x == 0x11223345;

	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
