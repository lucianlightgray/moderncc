#define s6_4_STR(x) #x
#define s6_4_XSTR(x) s6_4_STR(x)
#define s6_4_CAT(a, b) a%:%:b

static const char *s6_4_where(void) {

	return __func__;
}

void s6_4_lexical_test(void) {

	{
		int a = 1, b = 2;
		int r = a++ + b;
		printf("munch a+++b=%d a=%d\n", r, a);
	}

	printf("bases dec=%d oct=%d hex=%d\n", 255, 0377, 0xFF);
	printf("oct0777=%d hexcap=%d\n", 0777, 0Xabc);

	printf("sz 1U=%d 1L=%d 1LL=%d\n",
		   (int)sizeof(1U), (int)sizeof(1L), (int)sizeof(1LL));
	printf("sz maxhex=%d maxull=%d\n",
		   (int)sizeof(0xFFFFFFFFFFFFFFFF), (int)sizeof(18446744073709551615ULL));

	printf("hexf %d %d %d %d\n",
		   0x1.8p1 == 3.0, 0x1p-1 == 0.5, 0x1.0p4 == 16.0, 0x1p+4 == 16.0);
	printf("hexf2 %d %d\n", 0x0.8p1 == 1.0, 0x10p-4 == 1.0);

	printf("fltsz f=%d d=%d ldeq=%d\n",
		   (int)sizeof(1.0f), (int)sizeof(1.0),
		   (int)(sizeof(1.0L) == sizeof(long double)));

	printf("esc %d %d %d %d %d %d %d\n",
		   '\a', '\b', '\f', '\n', '\r', '\t', '\v');
	printf("esc2 q=%d dq=%d qm=%d bs=%d\n",
		   '\'', '\"', '\?', '\\');

	printf("num oct=%d hex=%d\n", '\101', '\x41');

	printf("charsz=%d val=%d\n", (int)sizeof('A'), 'A');

	{
		enum s6_4_e { s6_4_RED,
					  s6_4_GREEN = 10,
					  s6_4_BLUE };
		printf("enum %d %d %d sz=%d\n",
			   s6_4_RED, s6_4_GREEN, s6_4_BLUE, (int)sizeof(s6_4_GREEN));
	}

	printf("concat=[%s] len=%d\n", "ab"
								   "cd"
								   "ef",
		   (int)strlen("ab"
					   "cd"
					   "ef"));

	{
		char s[] = "\x12"
				   "3";
		printf("x12str len=%d b0=%d b1=%d\n",
			   (int)(sizeof(s) - 1), s[0], s[1]);
	}

	{
		int Ab = 3, aB = 7, AB = 9, ab = 1;
		printf("distinct %d %d %d %d\n", Ab, aB, AB, ab);
	}

	printf("func=%s here=%s\n", s6_4_where(), __func__);

	{
		int arr<:3:> = <%10, 20, 30%>;
		printf("digraph %d %d %d\n", arr<:0:>, arr<:1:>, arr<:2:>);
	}
	printf("paste %d\n", s6_4_CAT(12, 34));

	printf("ppnum=[%s]\n", s6_4_XSTR(0x1.2p3));

	printf("slashes=[%s] len=%d\n", "a//b", (int)strlen("a//b"));
	printf("aftercomment\n");
}
