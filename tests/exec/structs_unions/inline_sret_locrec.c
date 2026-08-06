extern int printf(const char *, ...);

typedef struct {
	int v;
} Small;

typedef struct {
	long a, b;
} Pair;

static const double _Complex cv[2] = {1, 2};
static double _Complex cslot[2];

static Small make_small(void) {
	Small l = {42};
	return l;
}

static Pair make_pair(void) {
	Pair p = {7, 9};
	return p;
}

int main(void) {
	int ok = 1;
	Small s = make_small();
	Pair p = make_pair();

	cslot[0] = cv[0] + cv[1];
	cslot[1] = cv[1] - cv[0];

	if (s.v != 42)
		ok = 0;
	if (p.a != 7 || p.b != 9)
		ok = 0;
	if (__real__ cslot[0] != 3.0 || __imag__ cslot[0] != 0.0)
		ok = 0;
	if (__real__ cslot[1] != 1.0 || __imag__ cslot[1] != 0.0)
		ok = 0;

	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
