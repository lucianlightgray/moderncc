extern int printf(const char *, ...);

/* T-lin-10480 part(2): tail-call-optimization type/width completeness. Each
 * function is a self-tail-recursive accumulator whose tail call is rewritten to
 * a loop by the tco pass, once per function. Six integer accumulator widths
 * (signed char/short/int/unsigned/long/long-long) plus the two register-class
 * FP accumulators (float, double) -> tco fires 8x. tco is driven by the call
 * being in tail position, not by the type, so all eight fire; the float/double
 * legs exercise the FP param-slot store path (closes the "tco FP-return"
 * residual). long double / __float128 stay excluded (x87 / soft-float param
 * slots -- the remaining residual). Results are deterministic triangular sums,
 * so -O0 and -O4 (and their JIT -run) must agree. */

signed char fc(signed char n, signed char acc){ if(n<=0) return acc; return fc((signed char)(n-1),(signed char)(acc+n)); }
short fs(short n, short acc){ if(n<=0) return acc; return fs((short)(n-1),(short)(acc+n)); }
int fi(int n, int acc){ if(n<=0) return acc; return fi(n-1, acc+n); }
unsigned fu(unsigned n, unsigned acc){ if(n==0) return acc; return fu(n-1, acc+n); }
long fl(long n, long acc){ if(n<=0) return acc; return fl(n-1, acc+n); }
long long fq(long long n, long long acc){ if(n<=0) return acc; return fq(n-1, acc+n); }
float ff(int n, float acc){ if(n<=0) return acc; return ff(n-1, acc + (float)n); }
double fd(int n, double acc){ if(n<=0) return acc; return fd(n-1, acc + (double)n); }

int main(void)
{
	/* fq SEEDS the accumulator high (5000000000LL) rather than recursing deep:
	 * the result 5050005000 stays > 2^32 (exercises the full 64-bit llong range,
	 * so an int-truncating long-long tco bug is still caught for anti-vacuity),
	 * while the recursion depth stays 10000 -- the -O0 baseline does NOT tco
	 * (recurses to full depth), and a >2^32 result via depth alone (sum(1..n) is
	 * O(n^2) -> n>~93000 frames) would overflow the -O0 stack env-marginally
	 * (SIGSEGV under larger env blocks / emulators). Seeding decouples "big
	 * 64-bit result" from "deep recursion"; tco firing is depth-independent so
	 * tco=8 is unchanged. (Fix + refinement: mac-arm64.) */
	printf("%d %d %d %u %ld %lld %.0f %.0f\n",
				 (int)fc(10, 0), (int)fs(20, 0), fi(100, 0), fu(50u, 0u),
				 fl(1000L, 0L), fq(10000LL, 5000000000LL), (double)ff(50, 0.0f), fd(100, 0.0));
	return 0;
}
