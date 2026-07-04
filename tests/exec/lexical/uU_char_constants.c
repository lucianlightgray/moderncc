typedef __CHAR16_TYPE__ char16_t;
typedef __CHAR32_TYPE__ char32_t;
extern int printf(const char *, ...);

int main(void) {
	char16_t a = u'A';
	char32_t b = U'Z';
	char16_t e = u'é';
	char32_t big = U'\U0001F600';
	int u = 1, U = 2;

	int ok = a == 65 && b == 90 && e == 0xe9 && big == 0x1F600 && sizeof(u'A') == sizeof(char16_t) && sizeof(U'A') == sizeof(char32_t) && u == 1 && U == 2;
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
