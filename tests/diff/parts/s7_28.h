#ifdef PARTS_STD_HAVE_UCHAR
void s7_28_uchar(void) {

	printf("uchar sizes %d %d\n", (int)sizeof(char16_t), (int)sizeof(char32_t));
	printf("uchar u/U %d %d\n", (int)u'A', (int)U'z');
#ifdef __STDC_UTF_16__
	printf("utf16 %d\n", (int)__STDC_UTF_16__);
#endif
#ifdef __STDC_UTF_32__
	printf("utf32 %d\n", (int)__STDC_UTF_32__);
#endif

	mbstate_t st32;
	char32_t c32 = 0;
	for (int i = 0; i < (int)sizeof(st32); i++)
		((char *)&st32)[i] = 0;
	size_t r = mbrtoc32(&c32, "A", 1, &st32);
	printf("mbrtoc32 %d %d\n", (int)r, (int)c32);

	char out[8];
	mbstate_t st32b;
	for (int i = 0; i < (int)sizeof(st32b); i++)
		((char *)&st32b)[i] = 0;
	size_t w = c32rtomb(out, 0x42, &st32b);
	printf("c32rtomb %d %d\n", (int)w, (int)out[0]);

	mbstate_t st16;
	char16_t c16 = 0;
	for (int i = 0; i < (int)sizeof(st16); i++)
		((char *)&st16)[i] = 0;
	size_t r16 = mbrtoc16(&c16, "Q", 1, &st16);
	printf("mbrtoc16 %d %d\n", (int)r16, (int)c16);
	char o16[8];
	mbstate_t st16b;
	for (int i = 0; i < (int)sizeof(st16b); i++)
		((char *)&st16b)[i] = 0;
	size_t w16 = c16rtomb(o16, 0x43, &st16b);
	printf("c16rtomb %d %d\n", (int)w16, (int)o16[0]);
}
#endif

void s7_28_wconv(void) {

	printf("btowc %d\n", (int)btowc('A'));
	printf("wctob %d\n", wctob(L'A'));
	mbstate_t st;
	for (int i = 0; i < (int)sizeof(st); i++)
		((char *)&st)[i] = 0;
	printf("mbsinit %d\n", mbsinit(&st) != 0);
	printf("mbsinit-null %d\n", mbsinit(NULL) != 0);

	wchar_t wc = 0;
	mbstate_t s2;
	for (int i = 0; i < (int)sizeof(s2); i++)
		((char *)&s2)[i] = 0;
	size_t rr = mbrtowc(&wc, "K", 1, &s2);
	printf("mbrtowc %d %d\n", (int)rr, (int)wc);
	char mb[8];
	mbstate_t s3;
	for (int i = 0; i < (int)sizeof(s3); i++)
		((char *)&s3)[i] = 0;
	size_t ww = wcrtomb(mb, L'L', &s3);
	printf("wcrtomb %d %d\n", (int)ww, (int)mb[0]);

	mbstate_t s4;
	for (int i = 0; i < (int)sizeof(s4); i++)
		((char *)&s4)[i] = 0;
	printf("mbrlen %d\n", (int)mbrlen("Z", 1, &s4));
}

static int s7_28_sgn(int r) {
	return (r > 0) - (r < 0);
}

void s7_28_wstr(void) {
	wchar_t buf[64];

	wcscpy(buf, L"Hello");
	printf("wcslen %d\n", (int)wcslen(buf));
	wcscat(buf, L" World");
	printf("wcscat-len %d\n", (int)wcslen(buf));

	printf("wcscmp %d %d %d\n",
		   s7_28_sgn(wcscmp(L"abc", L"abc")),
		   s7_28_sgn(wcscmp(L"abc", L"abd")),
		   s7_28_sgn(wcscmp(L"abd", L"abc")));
	printf("wcsncmp %d\n", s7_28_sgn(wcsncmp(L"abcX", L"abcY", 3)));

	static const wchar_t s6[] = L"abcabc";
	printf("wcschr %d\n", (int)(wcschr(s6, L'b') - s6 == 1));
	printf("wcsrchr %d\n", (int)(wcsrchr(s6, L'b') - s6 == 4));
	printf("wcschr-null %d\n", wcschr(L"abc", L'z') == NULL);

	static const wchar_t hw[] = L"hello world";
	printf("wcsstr %d\n", (int)(wcsstr(hw, L"wor") - hw == 6));
	printf("wcsstr-empty %d\n", wcsstr(hw, L"") == hw);

	printf("wcscspn %d\n", (int)wcscspn(L"abc123", L"0123456789"));
	printf("wcsspn %d\n", (int)wcsspn(L"12345abc", L"0123456789"));
	static const wchar_t df[] = L"abc-def";
	printf("wcspbrk %d\n", (int)(wcspbrk(df, L"-+") - df == 3));

	wchar_t nb[6];
	wcsncpy(nb, L"ab", 5);
	printf("wcsncpy %d %d %d\n", (int)nb[0], (int)nb[2], (int)nb[4]);

	wchar_t cb[16];
	wcscpy(cb, L"ab");
	wcsncat(cb, L"cdef", 2);
	printf("wcsncat %d\n", (int)wcslen(cb));

	wchar_t x1[32], x2[32];
	size_t l1 = wcsxfrm(x1, L"apple", 32);
	size_t l2 = wcsxfrm(x2, L"apple", 32);
	printf("wcsxfrm %d %d\n", (int)(l1 == l2), s7_28_sgn(wcscmp(x1, x2)));
}

void s7_28_wmem(void) {

	wchar_t a[8] = {1, 2, 3, 4, 5, 6, 7, 8}, b[8];
	wmemcpy(b, a, 4);
	printf("wmemcpy %d %d\n", (int)b[0], (int)b[3]);
	wmemmove(a + 1, a, 4);
	printf("wmemmove %d %d %d\n", (int)a[0], (int)a[1], (int)a[4]);
	wchar_t s[5];
	wmemset(s, 0x263A, 3);
	s[3] = 0;
	printf("wmemset %d %d\n", (int)s[0], (int)s[2]);

	wchar_t p[3] = {1, 2, 3}, q[3] = {1, 2, 4};
	printf("wmemcmp %d\n", s7_28_sgn(wmemcmp(p, q, 3)));
	printf("wmemchr %d\n", (int)(wmemchr(p, 2, 3) - p == 1));
}

void s7_28_wnum(void) {

	wchar_t *end;
	printf("wcstod %d\n", (int)(wcstod(L"2.5xyz", &end) * 2));
	printf("wcstod-end %d\n", *end == L'x');
	printf("wcstol %ld\n", wcstol(L"  -42", NULL, 10));
	printf("wcstoul %lu\n", wcstoul(L"0xff", NULL, 16));
	printf("wcstoll %lld\n", wcstoll(L"9000000000", NULL, 10));
	printf("wcstoull %llu\n", wcstoull(L"18000000000", NULL, 10));
	printf("wcstol-base0 %ld\n", wcstol(L"010", NULL, 0));
}

void s7_28_wprintf(void) {

	wchar_t buf[64];
	int n = swprintf(buf, 64, L"%d-%ls-%c", 7, L"hi", 'Z');
	printf("swprintf-ret %d\n", n);
	printf("swprintf-buf");
	for (int i = 0; buf[i]; i++)
		printf(" %d", (int)buf[i]);
	printf("\n");

	swprintf(buf, 64, L"%*d", 4, 5);
	printf("swprintf-width");
	for (int i = 0; buf[i]; i++)
		printf(" %d", (int)buf[i]);
	printf("\n");
	int a = 0, b = 0;
	int got = swscanf(L"12 34", L"%d %d", &a, &b);
	printf("swscanf %d %d %d\n", got, a, b);
}

void s7_28_wctypef(void) {

	printf("cls %d%d%d%d%d%d\n",
		   iswalpha(L'a') != 0, iswdigit(L'5') != 0, iswupper(L'A') != 0,
		   iswlower(L'z') != 0, iswspace(L' ') != 0, iswblank(L'\t') != 0);
	printf("cls2 %d%d%d%d%d%d\n",
		   iswalnum(L'k') != 0, iswpunct(L'!') != 0, iswcntrl(L'\n') != 0,
		   iswgraph(L'#') != 0, iswprint(L' ') != 0, iswxdigit(L'F') != 0);
	printf("clsF %d%d%d\n",
		   iswalpha(L'5') == 0, iswdigit(L'a') == 0, iswspace(L'x') == 0);

	printf("wctype %d %d\n",
		   iswctype(L'q', wctype("alpha")) != 0,
		   iswctype(L'5', wctype("digit")) != 0);
	printf("wctype-bad %d\n", wctype("nosuchclass") == 0);

	printf("towlower %d\n", (int)towlower(L'Z'));
	printf("towupper %d\n", (int)towupper(L'a'));
	printf("towlower-id %d\n", (int)towlower(L'5'));
	printf("towctrans %d %d\n",
		   (int)towctrans(L'm', wctrans("toupper")),
		   (int)towctrans(L'M', wctrans("tolower")));
	printf("wctrans-bad %d\n", wctrans("nope") == 0);
}

void s7_28_wtok(void) {

	wchar_t s[] = L"a,bb,,ccc";
	wchar_t *save = NULL, *tok;
	tok = wcstok(s, L",", &save);
	printf("wcstok");
	while (tok) {
		printf(" %d", (int)wcslen(tok));
		tok = wcstok(NULL, L",", &save);
	}
	printf("\n");
}
