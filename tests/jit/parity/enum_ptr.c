extern int printf(const char *, ...);

enum col { RED = 1, GREEN = 2, BLUE = 4 };

struct pal {
	enum col fg;
	enum col bg;
};

static int blend(enum col *a, struct pal *p, int n)
{
	int s = 0;
	int i;
	for (i = 0; i < n; i++)
		s += (int)*a * (i & 7) + (int)p->fg - (int)p->bg;
	*a = (enum col)((s & 1) ? GREEN : BLUE);
	return s;
}

int main(void)
{
	enum col c = GREEN;
	struct pal p;
	long t = 0;
	int i;
	p.fg = RED;
	p.bg = BLUE;
	for (i = 0; i < 300000; i++)
		t += blend(&c, &p, 8);
	printf("blend %ld %d\n", t, (int)c);
	return 0;
}
