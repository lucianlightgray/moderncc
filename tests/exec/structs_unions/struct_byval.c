#include <stdio.h>
#include <string.h>

typedef struct {
	int a, b;
} S2i;
typedef struct {
	int a;
	double d;
} Sid;
typedef struct {
	double x, y;
} S2d;
typedef struct {
	long a, b, c;
} S3l;
typedef struct {
	float a, b, c, d;
} S4f;
typedef struct {
	S2i lo, hi;
} Snest;

static void dump(const char *t, const void *p, int n) {
	const unsigned char *b = p;
	printf("%s:", t);
	for (int i = 0; i < n; i++)
		printf(" %02x", b[i]);
	printf("\n");
}

static S2i f1(S2i s) {
	s.a += 1;
	s.b += 2;
	return s;
}
static Sid f2(Sid s) {
	s.a ^= 0x5a5a;
	s.d = -s.d;
	return s;
}
static S2d f3(S2d s) {
	double t = s.x;
	s.x = s.y;
	s.y = t;
	return s;
}
static S3l f4(S3l s) {
	s.a += s.b;
	s.b += s.c;
	s.c += s.a;
	return s;
}
static S4f f5(S4f s) {
	S4f r = {s.d, s.c, s.b, s.a};
	return r;
}
static Snest f6(Snest s) {
	s.lo = f1(s.lo);
	s.hi = f1(f1(s.hi));
	return s;
}

int main(void) {
	S2i a;
	memset(&a, 0, sizeof a);
	a.a = 10;
	a.b = 20;
	a = f1(f1(a));
	printf("S2i %d %d\n", a.a, a.b);
	dump("S2i", &a, sizeof a);

	Sid b;
	memset(&b, 0, sizeof b);
	b.a = 0x1234;
	b.d = 3.5;
	b = f2(b);
	printf("Sid %d %g\n", b.a, b.d);
	dump("Sid", &b, sizeof b);

	S2d c;
	memset(&c, 0, sizeof c);
	c.x = 1.5;
	c.y = 2.5;
	c = f3(f3(c));
	printf("S2d %g %g\n", c.x, c.y);
	dump("S2d", &c, sizeof c);

	S3l d;
	memset(&d, 0, sizeof d);
	d.a = 1;
	d.b = 2;
	d.c = 3;
	d = f4(d);
	printf("S3l %ld %ld %ld\n", d.a, d.b, d.c);
	dump("S3l", &d, sizeof d);

	S4f e;
	memset(&e, 0, sizeof e);
	e.a = 1;
	e.b = 2;
	e.c = 3;
	e.d = 4;
	e = f5(f5(e));
	printf("S4f %g %g %g %g\n", e.a, e.b, e.c, e.d);
	dump("S4f", &e, sizeof e);

	Snest f;
	memset(&f, 0, sizeof f);
	f.lo.a = 1;
	f.lo.b = 1;
	f.hi.a = 2;
	f.hi.b = 2;
	f = f6(f);
	printf("Snest %d %d %d %d\n", f.lo.a, f.lo.b, f.hi.a, f.hi.b);
	return 0;
}
