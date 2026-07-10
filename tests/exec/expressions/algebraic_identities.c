#include <stdio.h>

volatile int v = 40;
volatile unsigned uv = 0xfffffff0u;
volatile long long lv = 1234567890123LL;
static int ncalls;

static int f(void) {
	ncalls++;
	printf("f\n");
	return 7;
}

static int add0(int x) { return x + 0; }
static int add0l(int x) { return 0 + x; }
static int sub0(int x) { return x - 0; }
static int mul1(int x) { return x * 1; }
static int mul1l(int x) { return 1 * x; }
static int div1(int x) { return x / 1; }
static int shl0(int x) { return x << 0; }
static int shr0(int x) { return x >> 0; }
static int or0(int x) { return x | 0; }
static int xor0(int x) { return x ^ 0; }
static int andm1(int x) { return x & -1; }
static int and0(int x) { return x & 0; }
static int mul0(int x) { return x * 0; }
static int mul0l(int x) { return 0 * x; }
static int subx(int x) { return x - x; }
static int xorx(int x) { return x ^ x; }
static int andx(int x) { return x & x; }
static int orx(int x) { return x | x; }
static int orm1(int x) { return x | -1; }
static int cascade(int x) { return (x + 0) - x; }
static int cascade2(int x) { return (x - x) + 5; }
static int comp0(int a, int b) { int t = a + b; return t ^ t; }
static int compz(int a, int b) { return (a * 3 + b) * 0; }
static unsigned uadd0(unsigned u) { return u + 0; }
static unsigned umul1(unsigned u) { return u * 1; }
static unsigned uandm1(unsigned u) { return u & 0xffffffffu; }
static unsigned uor0(unsigned u) { return u | 0u; }
static unsigned usub(unsigned u) { return u - u; }
static unsigned uand(unsigned u) { return u & u; }
static unsigned mixsign(int n) { return (n + 0u) / 2; }
static long long ladd0(long long l) { return l + 0; }
static long long lmul1(long long l) { return l * 1; }
static long long lsub(long long l) { return l - l; }
static long long lxor(long long l) { return l ^ l; }
static long long ladd0ll(long long l) { return l + 0LL; }
static long long landm1(long long l) { return l & -1LL; }
static long long lshl0(long long l) { return l << 0; }
static int vadd0(void) { return v + 0; }
static int vmul1(void) { return v * 1; }
static int vshl0(void) { return v << 0; }
static int vmul0(void) { return v * 0; }
static int vsub(void) { return v - v; }
static int callmul0(void) { return f() * 0; }
static int callmul0l(void) { return 0 * f(); }
static int calland0(void) { return f() & 0; }
static int callorm1(void) { return f() | -1; }
static int callsub(void) { return f() - f(); }
static int calland(void) { return f() & f(); }
static int incmul0(int y) {
	int r = (++y) * 0;
	return r + y;
}
static int loadsub(int *p, int i) { return p[i] - p[i]; }
static int loadmul0(int *p, int i) { return p[i] * 0; }
static int deadbranch(int x) {
	if (x - x)
		return 1;
	return 2;
}
static int cadd0(signed char c) { return c + 0; }
static int cmul1(signed char c) { return c * 1; }
static int ucadd0(unsigned char uc) { return uc + 0; }
static int ucand(unsigned char uc) { return uc & 0xff; }

int main(void) {
	int x = v;
	unsigned u = uv;
	long long l = lv;

	printf("%d %d %d %d %d\n", add0(x), add0l(x), sub0(x), mul1(x), mul1l(x));
	printf("%d %d %d %d %d\n", div1(x), shl0(x), shr0(x), or0(x), xor0(x));
	printf("%d %d %d %d\n", andm1(x), and0(x), mul0(x), mul0l(x));
	printf("%d %d %d %d\n", subx(x), xorx(x), andx(x), orx(x));
	printf("%d %d %d\n", orm1(x), cascade(x), cascade2(x));
	printf("%d %d\n", comp0(x, 3), compz(x, 5));
	printf("%u %u %u %u\n", uadd0(u), umul1(u), uandm1(u), uor0(u));
	printf("%u %u\n", usub(u), uand(u));
	printf("%u\n", mixsign(-x));
	printf("%lld %lld %lld %lld\n", ladd0(l), lmul1(l), lsub(l), lxor(l));
	printf("%lld %lld %lld\n", ladd0ll(l), landm1(l), lshl0(l));
	printf("%d %d %d\n", vadd0(), vmul1(), vshl0());
	printf("%d %d\n", vmul0(), vsub());
	printf("%d\n", callmul0());
	printf("%d\n", callmul0l());
	printf("%d\n", calland0());
	printf("%d\n", callorm1());
	printf("%d\n", callsub());
	printf("%d\n", calland());
	printf("%d\n", ncalls);
	printf("%d\n", incmul0(3));
	int arr[4] = {1, 2, 3, 4};
	printf("%d %d\n", loadsub(arr, x & 3), loadmul0(arr, x & 3));
	printf("%d\n", deadbranch(x));
	printf("%d %d\n", cadd0(-5), cmul1(-5));
	printf("%d %d\n", ucadd0(200), ucand(200));
	return 0;
}
