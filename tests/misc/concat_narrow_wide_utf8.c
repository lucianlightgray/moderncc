#include <stddef.h>

static wchar_t a[] = "aé" L"b";
static unsigned short b[] = "é" u"x";
static unsigned int c[] = "é" U"x";
static wchar_t d[] = L"a" "bé";
static unsigned short s[] = "\U0001F600" u"x";
static unsigned int w[] = "\U0001F600" U"x";
static wchar_t asc[] = "ab" L"cd";
static wchar_t three[] = "é" L"β" "z";

int main(void) {
	if (sizeof(a) / sizeof(a[0]) != 4) return 1;
	if (a[0] != 97 || a[1] != 233 || a[2] != 98 || a[3] != 0) return 2;

	if (sizeof(b) / sizeof(b[0]) != 3) return 3;
	if (b[0] != 233 || b[1] != 120 || b[2] != 0) return 4;

	if (sizeof(c) / sizeof(c[0]) != 3) return 5;
	if (c[0] != 233 || c[1] != 120 || c[2] != 0) return 6;

	if (sizeof(d) / sizeof(d[0]) != 4) return 7;
	if (d[0] != 97 || d[1] != 98 || d[2] != 233 || d[3] != 0) return 8;

	if (sizeof(s) / sizeof(s[0]) != 4) return 9;
	if (s[0] != 55357 || s[1] != 56832 || s[2] != 120 || s[3] != 0) return 10;

	if (sizeof(w) / sizeof(w[0]) != 3) return 11;
	if (w[0] != 128512u || w[1] != 120 || w[2] != 0) return 12;

	if (sizeof(asc) / sizeof(asc[0]) != 5) return 13;
	if (asc[0] != 97 || asc[1] != 98 || asc[2] != 99 || asc[3] != 100 || asc[4] != 0) return 14;

	if (sizeof(three) / sizeof(three[0]) != 4) return 15;
	if (three[0] != 233 || three[1] != 946 || three[2] != 122 || three[3] != 0) return 16;

	return 0;
}
