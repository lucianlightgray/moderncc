extern int printf(const char *, ...);

typedef __WCHAR_TYPE__ wchar_t;

static int wlen(const wchar_t *s) {
	int n = 0;
	while (s[n])
		n++;
	return n;
}
static int slen(const char *s) {
	int n = 0;
	while (s[n])
		n++;
	return n;
}

int main(void) {
	int ok = 1;
	const wchar_t *s = L"ab"
										 "cd";
	const wchar_t *s2 = L"xy"
											L"zw";
	const char *n = "ab"
									"cd"
									"ef";
	wchar_t w[] = L"pq"
								"rs";

	const wchar_t *nf = "ab"
											L"cd";
	const wchar_t *nf2 = "x"
											 "y"
											 L"z";
	wchar_t wnf[] = "x"
									L"y";
	wchar_t wnf3[3] = "p"
										L"q";

	if (!(s[0] == (wchar_t)'a' && s[1] == (wchar_t)'b' && s[2] == (wchar_t)'c' && s[3] == (wchar_t)'d' && s[4] == 0 && wlen(s) == 4))
		ok = 0;
	if (wlen(s2) != 4)
		ok = 0;
	if (slen(n) != 6)
		ok = 0;
	if (!(w[3] == (wchar_t)'s' && wlen(w) == 4))
		ok = 0;
	if (!(nf[0] == (wchar_t)'a' && nf[3] == (wchar_t)'d' && wlen(nf) == 4))
		ok = 0;
	if (!(nf2[0] == (wchar_t)'x' && nf2[2] == (wchar_t)'z' && wlen(nf2) == 3))
		ok = 0;
	if (!(wnf[0] == (wchar_t)'x' && wnf[1] == (wchar_t)'y' && wlen(wnf) == 2))
		ok = 0;
	if (!(wnf3[0] == (wchar_t)'p' && wnf3[1] == (wchar_t)'q' && wnf3[2] == 0))
		ok = 0;

	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
