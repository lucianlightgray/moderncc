extern int printf(const char *, ...);

struct Box { int lo, hi; };

int ga = 10, gb = 20;

int main(void)
{
	struct Box b;
	b.lo = ga;
	b.hi = gb;
	int span = b.hi - b.lo;
	int mid = (b.hi + b.lo) / 2;
	printf("%d %d\n", span, mid);
	return 0;
}
