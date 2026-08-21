/* T-win-50041 -O1 crash bisection (arm64-Windows only). Round 4: the u64-limb
   helpers do NOT crash; the FULL probe (30/30) does — so isolate the __int256
   TYPE operations. Each PART is an external fn codegen'd by -c (empty main). */
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

#ifdef PART_I256_CASTV   /* runtime double -> __int256 */
int f_castv(double x) { __int256 v = (__int256)x; L *l = (L *)&v; return (int)l[0]; }
#endif

#ifdef PART_I256_CASTC   /* compile-time (const-fold) double -> __int256 */
void f_castc(L *o) { __int256 v = (__int256)-1e30; L *l = (L *)&v; o[0] = l[0]; o[3] = l[3]; }
#endif

#ifdef PART_I256_NEG     /* __int256 unary minus */
void f_neg(L *o, __int256 a) { __int256 v = -a; L *l = (L *)&v; o[0] = l[0]; }
#endif

#ifdef PART_I256_ALIAS   /* p256's pattern: read an __int256-by-value param via L* */
void f_alias(L *o, __int256 v) { L *l = (L *)&v; o[0] = l[0]; o[1] = l[1]; o[2] = l[2]; o[3] = l[3]; }
#endif

#ifdef PART_I256_ADD     /* __int256 arithmetic */
void f_add(L *o, __int256 a, __int256 b) { __int256 v = a + b; L *l = (L *)&v; o[0] = l[0]; }
#endif

int main(void) { return 0; }
