/* T-win-50041 -O1 crash bisection (arm64-Windows only). Compile each PART with
   -DPART_* -O1 on the windows-11-arm runner to find the minimal construct that
   segfaults mcc's arm64 -O1 codegen. Mirrors woa-int256-probe.c's helpers. */
typedef unsigned long long L;

#ifdef PART_SHL
void t_shl(L r[4], unsigned n) {
	unsigned limb = n / 64, bit = n % 64;
	L out[4] = {0, 0, 0, 0};
	int i;
	for (i = 3; i >= 0; i--) {
		int si = i - (int)limb;
		if (si >= 0) {
			out[i] |= r[si] << bit;
			if (bit && si - 1 >= 0) out[i] |= r[si - 1] >> (64 - bit);
		}
	}
	for (i = 0; i < 4; i++) r[i] = out[i];
}
#endif

#ifdef PART_NEG
void t_neg(L r[4], const L a[4]) {
	L z[4] = {0, 0, 0, 0}, borrow = 0;
	int i;
	for (i = 0; i < 4; i++) {
		L d = z[i] - a[i], b1 = z[i] < a[i], t = d - borrow, b2 = d < borrow;
		r[i] = t;
		borrow = b1 | b2;
	}
}
#endif

#ifdef PART_MAG
extern void t_shl(L r[4], unsigned n);
void t_mag(L r[4], double x) {
	int i, exp, e;
	L b, mant;
	__builtin_memcpy(&b, &x, 8);
	for (i = 0; i < 4; i++) r[i] = 0;
	if (!(x >= 1.0)) return;
	exp = (int)((b >> 52) & 0x7FF) - 1023;
	mant = (b & 0xFFFFFFFFFFFFFULL) | ((L)1 << 52);
	e = exp - 52;
	if (exp >= 256) return;
	r[0] = mant;
#ifdef PART_MAG_CALLS_SHL
	if (e > 0) t_shl(r, (unsigned)e);
#endif
}
#endif

#ifdef PART_VARSHIFT
/* isolate just a single runtime-variable 64-bit shift in a loop */
void t_varshift(L r[4], unsigned bit) {
	int i;
	for (i = 0; i < 4; i++)
		r[i] = (r[i] << bit) | (r[i] >> (64 - bit));
}
#endif

int main(void) { return 0; }
