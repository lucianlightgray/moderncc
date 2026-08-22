extern int printf(const char *, ...);

signed char gcc1 = 3, gcc2 = 9, gca = 100, gcb = -50;
short gs1 = 3, gs2 = 9, gsa = 300, gsb = -200;
int gi1 = 3, gi2 = 9, gia = 41, gib = 17;
unsigned gu1 = 3, gu2 = 9, gua = 4000000000u, gub = 7;
long gl1 = 3, gl2 = 9, gla = 5000006, glb = 6;
long long gll1 = 3, gll2 = 9, glla = 9000000000000LL, gllb = 8;

int main(void)
{
	signed char c1 = gcc1, c2 = gcc2, ca = gca, cb = gcb;
	short s1 = gs1, s2 = gs2, sa = gsa, sb = gsb;
	int i1 = gi1, i2 = gi2, ia = gia, ib = gib;
	unsigned u1 = gu1, u2 = gu2, ua = gua, ub = gub;
	long l1 = gl1, l2 = gl2, la = gla, lb = glb;
	long long q1 = gll1, q2 = gll2, qa = glla, qb = gllb;

	signed char rc = c1 < c2 ? ca : cb;
	short rs = s1 < s2 ? sa : sb;
	int ri = i1 < i2 ? ia : ib;
	unsigned ru = u1 < u2 ? ua : ub;
	long rl = l1 < l2 ? la : lb;
	long long rq = q1 < q2 ? qa : qb;

	printf("%d %d %d %u %ld %lld\n", (int)rc, (int)rs, ri, ru, rl, rq);
	return 0;
}
