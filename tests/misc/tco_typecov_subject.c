extern int printf(const char *, ...);

/* T-lin-10480 part(2): tail-call-optimization type/width completeness. Each
 * function is a self-tail-recursive accumulator; the tail call is rewritten to
 * a loop by the tco pass, once per function -> once per return-type width. The
 * distinct widths are char/short/int/unsigned/long/long-long (tco is driven by
 * the call being in tail position, not by the return type, so all six fire).
 * Results are deterministic triangular sums, so -O0 and -O4 (and their JIT
 * -run) must agree. FP return types (float/double/long double) are the known
 * residual -- see T-lin-10480 / DETAILS. */

signed char fc(signed char n, signed char acc){ if(n<=0) return acc; return fc((signed char)(n-1),(signed char)(acc+n)); }
short fs(short n, short acc){ if(n<=0) return acc; return fs((short)(n-1),(short)(acc+n)); }
int fi(int n, int acc){ if(n<=0) return acc; return fi(n-1, acc+n); }
unsigned fu(unsigned n, unsigned acc){ if(n==0) return acc; return fu(n-1, acc+n); }
long fl(long n, long acc){ if(n<=0) return acc; return fl(n-1, acc+n); }
long long fq(long long n, long long acc){ if(n<=0) return acc; return fq(n-1, acc+n); }

int main(void)
{
	printf("%d %d %d %u %ld %lld\n",
				 (int)fc(10, 0), (int)fs(20, 0), fi(100, 0), fu(50u, 0u),
				 fl(1000L, 0L), fq(100000LL, 0LL));
	return 0;
}
