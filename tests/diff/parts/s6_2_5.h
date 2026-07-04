#define s6_2_5_gtag(x) _Generic((x), \
	char: 1,                         \
	signed char: 2,                  \
	unsigned char: 3,                \
	default: 0)

enum s6_2_5_e { s6_2_5_A,
				s6_2_5_B,
				s6_2_5_C };

extern int s6_2_5_arr[];
int s6_2_5_arr[5];

void s6_2_5_types(void) {

	printf("bool: %d %d %d %d %d\n",
		   (int)(_Bool)5, (int)(_Bool)0, (int)(_Bool)0.3,
		   (int)(_Bool)0.0, (int)(_Bool)-2);

	{
		char c = 0;
		signed char sc = 0;
		unsigned char uc = 0;
		printf("chardistinct: %d %d %d\n",
			   s6_2_5_gtag(c), s6_2_5_gtag(sc), s6_2_5_gtag(uc));
		printf("charsize: %d %d %d\n",
			   (int)sizeof(char), (int)sizeof(signed char),
			   (int)sizeof(unsigned char));
	}

	{
		unsigned u = UINT_MAX;
		unsigned char ucv = 255;
		printf("umod: %d %d\n",
			   (u + 1u) == 0u,
			   (unsigned char)(ucv + 1) == 0);
	}

	{
		int i = 1234567;
		unsigned u;
		memcpy(&u, &i, sizeof u);
		printf("nonnegrep: %d\n", u == (unsigned)i);
	}

	{
		int i = 0x1234ABCD, j;
		unsigned char b[sizeof(int)];
		memcpy(b, &i, sizeof i);
		memcpy(&j, b, sizeof j);
		printf("objrep: %d %d\n", (int)sizeof b == (int)sizeof(int), i == j);
	}

	printf("twoscomp: %d\n", INT_MIN == -INT_MAX - 1);

	printf("ucharmax: %d %d\n", UCHAR_MAX == (1 << CHAR_BIT) - 1, CHAR_BIT == 8);

	{
		double _Complex cd;
		double a[2];
		__real__ cd = 3.0;
		__imag__ cd = 4.0;
		memcpy(a, &cd, sizeof a);
		printf("cplxlayout: %d %d %d\n",
			   sizeof(double _Complex) == sizeof(double[2]),
			   a[0] == 3.0, a[1] == 4.0);
	}

	printf("align: %d %d %d\n",
		   _Alignof(char) == 1,
		   _Alignof(float[2]) == _Alignof(float),
		   _Alignof(double _Complex) == _Alignof(double));

	printf("maxalign: %d\n", _Alignof(max_align_t) >= _Alignof(double));

	{
		_Alignas(16) unsigned char buf[16];
		printf("overalign: %d\n", ((unsigned long)(size_t)&buf[0] % 16u) == 0);
	}

	{
		_Alignas(0) int x = 7;
		printf("alignaszero: %d %d\n", _Alignof(x) == _Alignof(int), x == 7);
	}

	printf("composite: %d\n", (int)sizeof(s6_2_5_arr) == 5 * (int)sizeof(int));

	{
		enum s6_2_5_e e = s6_2_5_B;
		int i = e;
		e = (enum s6_2_5_e)(i + 1);
		printf("enum: %d %d\n", i == 1, e == s6_2_5_C);
	}
}
