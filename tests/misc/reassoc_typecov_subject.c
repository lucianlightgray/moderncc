extern int printf(const char *, ...);

/* T-lin-10480 part(2): reassociation type/width completeness. `v*3 + v*5`
 * mul-distributes to `v*8` per the reassoc pass. Integer arithmetic promotes
 * char/short to int, so the distinct reassoc widths are int/unsigned/long/
 * long-long; char/short are included for shape completeness (they reassoc as
 * promoted int). Globals block constant folding; the printed result is
 * deterministic so -O0 and -O4 (and their JIT -run) must agree. */

signed char gc = 6;
short gs = 7;
int gi = 41;
unsigned gu = 4000000000u;
long gl = 5000006L;
long long gq = 9000000000000LL;

int main(void)
{
	signed char c = gc;
	short s = gs;
	int i = gi;
	unsigned u = gu;
	long l = gl;
	long long q = gq;

	signed char rc = (signed char)(c * 3 + c * 5);
	short rs = (short)(s * 3 + s * 5);
	int ri = i * 3 + i * 5;
	unsigned ru = u * 3 + u * 5;
	long rl = l * 3 + l * 5;
	long long rq = q * 3 + q * 5;

	printf("%d %d %d %u %ld %lld\n", (int)rc, (int)rs, ri, ru, rl, rq);
	return 0;
}
