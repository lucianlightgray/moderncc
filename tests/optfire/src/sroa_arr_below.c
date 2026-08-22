extern int printf(const char *, ...);

struct Pt { int a, b; };

int g0 = 1, g1 = 2, g2 = 3, g3 = 4;

int main(void)
{
	struct Pt u, v;
	u.a = g0;
	u.b = g1;
	v.a = g2;
	v.b = g3;
	int dot = u.a * v.a + u.b * v.b;
	int cross = u.a * v.b - u.b * v.a;
	printf("%d %d\n", dot, cross);
	return 0;
}
