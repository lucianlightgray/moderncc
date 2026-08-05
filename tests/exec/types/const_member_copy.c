extern int printf(const char *, ...);

typedef struct {
	const char *label;
	const int value;
} Section;

typedef struct {
	const int v;
} Inner;

static const Section sections[2] = {{"a", 0}, {"b", 1}};
static const double _Complex cv[2] = {1, 2};

double _Complex *cp;
static double _Complex cslot[2];

static int take_by_value(Section s) {
	return s.value;
}

static Inner make(void) {
	Inner l = {42};
	return l;
}

static int sum_varargs(int n, ...) {
	return n;
}

int main(void) {
	int ok = 1;
	Inner i = make();

	if (take_by_value(sections[1]) != 1)
		ok = 0;
	if (i.v != 42)
		ok = 0;
	if (sum_varargs(1, sections[0]) != 1)
		ok = 0;

	cp = cslot;
	cp[0] = cv[0] + cv[1];
	if (__real__ cp[0] != 3.0 || __imag__ cp[0] != 0.0)
		ok = 0;

	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
