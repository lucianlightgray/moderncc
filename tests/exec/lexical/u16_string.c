typedef __CHAR16_TYPE__ char16_t;
extern int printf(const char *, ...);

char16_t g[] = u"Hi";

int main(void) {
	char16_t s[] = u"ABC";
	const char16_t *p = u"xy";
	char16_t cat[] = u"a"
					 u"b";
	char16_t emoji[] = u"\U0001F600";

	int ok = sizeof(char16_t) == 2 && sizeof(s) == 4 * sizeof(char16_t) && s[0] == 65 && s[2] == 67 && s[3] == 0 && p[0] == (char16_t)'x' && p[1] == (char16_t)'y' && p[2] == 0 && g[0] == (char16_t)'H' && g[2] == 0 && cat[0] == (char16_t)'a' && cat[1] == (char16_t)'b' && cat[2] == 0 && sizeof(emoji) == 3 * sizeof(char16_t) && emoji[0] == 0xD83D && emoji[1] == 0xDE00 && emoji[2] == 0;
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
