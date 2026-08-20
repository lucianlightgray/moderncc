extern int printf(const char *, ...);

typedef float v4f __attribute__((vector_size(16)));
typedef double v2d __attribute__((vector_size(16)));
typedef int v4i __attribute__((vector_size(16)));

static v4f fadd(v4f a, v4f b) { return a + b; }
static v2d dmul(v2d a, v2d b) { return a * b; }
static v4i iadd(v4i a, v4i b) { return a + b; }
static v4i iand(v4i a, v4i b) { return a & b; }
static v4i imul(v4i a, v4i b) { return a * b; }
static v4f fcmplt(v4f a, v4f b) { return (v4f)(a < b); }
static v4i icmplt(v4i a, v4i b) { return a < b; }

int main(void)
{
	v4f a = {1, 2, 3, 4}, b = {4, 3, 2, 1};
	v2d c = {2, 3}, d = {5, 7};
	v4i e = {10, 20, 30, 40}, f = {6, 20, 3, 40};

	v4f fa = fadd(a, b);
	v2d dm = dmul(c, d);
	v4i ia = iadd(e, f);
	v4i an = iand(e, f);
	v4i im = imul(e, f);
	v4f fc = fcmplt(a, b);
	v4i ic = icmplt(f, e);

	printf("%.0f %.0f %.0f %d %d %d %d %d %d %d\n",
				 fa[0] + fa[3], dm[0] + dm[1], (double)(ia[0] + ia[3]),
				 an[1], im[0] + im[3], (int)fc[0], (int)fc[3], ic[0], ic[1], ic[3]);
	return 0;
}
