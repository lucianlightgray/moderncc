extern int printf(const char *, ...);

/* T-lin-10480 part(2): value-range (VRP) type/width completeness. A bounded
 * compare `v>=10 && v<=20` drives the range pass across each integer width.
 * The 5 SIGNED widths (signed char/short/int/long/long-long) range-fire;
 * UNSIGNED is excluded — the range pass does not fire on unsigned bounded
 * compares (VRP treats the wrap domain differently), a real per-pass shape,
 * not a width bug. Globals block folding; the result (all 6 conditions true =
 * 63) is deterministic so -O0/-O4 and their JIT -run must agree. */

signed char gc = 14;
short gs = 14;
int gi = 14;
unsigned gu = 14;
long gl = 14;
long long gq = 14;

int main(void)
{
	int r = 0;
	{ signed char v = gc; if (v >= 10 && v <= 20) r += 1; }
	{ short v = gs; if (v >= 10 && v <= 20) r += 2; }
	{ int v = gi; if (v >= 10 && v <= 20) r += 4; }
	{ unsigned v = gu; if (v >= 10u && v <= 20u) r += 8; }
	{ long v = gl; if (v >= 10L && v <= 20L) r += 16; }
	{ long long v = gq; if (v >= 10LL && v <= 20LL) r += 32; }
	printf("%d\n", r);
	return 0;
}
