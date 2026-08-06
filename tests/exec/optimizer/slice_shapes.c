/* Shapes the maximal-call-free-slice enumerator (MCC_SLICE_CENSUS,
   tools/slice-census.py) has to partition correctly, exercised for value so a
   miscount that comes with a codegen change is caught here too.

   Covered: a call-free loop nest that must stay one slice; a loop whose body
   calls out, so the loop is a boundary but its increment is not; a run of
   plain statements between two calls; an indirect call through a function
   pointer (never transparent); a static leaf callee (graftable from -O2, so
   the two slices around it merge in the t=1 partition); switch, ternary,
   do-while and nested if inside a slice. */

#include <stdio.h>

static int leaf(int x) { return x * 3 + 1; }

extern int opaque(int);

int opaque(int x) { return x ^ 0x5a; }

static int nest(const int *a, int n) {
	int s = 0, i, j;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			s += a[i] * a[j] + (i ^ j);
	return s;
}

static int calls_in_loop(const int *a, int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += opaque(a[i]) + i;
	return s;
}

static int between_calls(int x) {
	int a, b, c;
	a = opaque(x);
	b = a * 7;
	c = b - 3;
	b = c > 100 ? c - 100 : c + 100;
	switch (b & 3) {
	case 0:
		c += 1;
		break;
	case 1:
		c += 2;
		break;
	default:
		c += 3;
		break;
	}
	do {
		c -= 5;
	} while (c > 200);
	if (c < 0) {
		if (b > 0)
			c = -c;
		else
			c = 0;
	}
	return opaque(c) + b;
}

static int indirect(int (*fp)(int), int x) {
	int s = 0, i;
	for (i = 0; i < 4; i++)
		s += fp(x + i);
	for (i = 0; i < 4; i++)
		s += leaf(x + i);
	return s;
}

int main(void) {
	int a[8], i;
	for (i = 0; i < 8; i++)
		a[i] = i * i - 3;
	printf("nest=%d\n", nest(a, 8));
	printf("calls_in_loop=%d\n", calls_in_loop(a, 8));
	printf("between_calls=%d\n", between_calls(11));
	printf("indirect=%d\n", indirect(opaque, 5));
	printf("leaf=%d\n", leaf(9));
	return 0;
}
