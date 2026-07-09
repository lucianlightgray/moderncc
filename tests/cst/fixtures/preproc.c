#define SQUARE(x) ((x) * (x))
#define ENABLE_A 1

#if ENABLE_A
static int mode = 1;
#else
this text is inside an inactive branch and must round-trip byte-for-byte $$$
#endif

#ifndef MISSING
#define FALLBACK 42
#endif

int use_macros(int v) {
	int a = SQUARE(v + 1);
	int b = FALLBACK;
	return a + b + mode;
}
