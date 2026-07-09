static int s6_3_taken;
static int s6_3_side(int v) {
	s6_3_taken = v;
	return v;
}

void s6_3_conversions(void) {

	printf("bool: %d %d %d %d %d\n",
				 (int)(_Bool)2.5, (int)(_Bool)0.0, (int)(_Bool)-0.0,
				 (int)(_Bool)(void *)0, (int)(_Bool)(char)256);

	printf("umod: %d %lu %d\n",
				 (int)(unsigned char)257, (unsigned long)((unsigned)-1) & 0xFFFFFFFFul,
				 (int)(unsigned short)65537);

	printf("snarrow: %d %d\n", (int)(signed char)300, (int)(signed char)-200);

	printf("ftrunc: %d %d %d %d\n",
				 (int)3.9, (int)-3.9, (int)0.99, (int)-0.99);

	printf("i2f: %d %d\n", (double)16777216 == 16777216.0, (float)1 == 1.0f);

	printf("widen: %d %d\n",
				 (double)(float)1.0f == 1.0, (double)(float)0.5f == 0.5);

	printf("narrow: %d\n", (float)0.1 != 0.1);

	{
		double _Complex z = 5.0 + 9.0 * I;
		printf("c2r: %d\n", (double)z == 5.0);
	}

	{
		double _Complex z = 7.0;
		printf("r2c: %d %d\n", creal(z) == 7.0, cimag(z) == 0.0);
	}

	printf("uac: %d %d\n",
				 (-1 < 0u),
				 ((long)-1 < 0u));

	{
		long long big = 0x100000000LL;
		int i = 1;
		printf("rank: %lld\n", big + i);
	}

	{
		int a[3] = {11, 22, 33};
		int *p = a;
		printf("decay: %d %d\n", *p, sizeof(a) == 3 * sizeof(int));
	}

	{
		int (*fp)(int) = s6_3_side;
		printf("fdecay: %d\n", fp(42));
	}

	s6_3_taken = 0;
	(void)s6_3_side(99);
	printf("void: %d\n", s6_3_taken);

	{
		int obj = 0;
		int *np = 0;
		printf("null: %d %d\n", np == (void *)0, ((void *)0 == (char *)0));
		printf("nulldiff: %d\n", np != &obj);
	}

	{
		int obj = 5;
		void *vp = &obj;
		int *ip = vp;
		printf("voidp: %d\n", ip == &obj);
	}

	{
		unsigned int u = 0x04030201u;
		unsigned char *b = (unsigned char *)&u;
		printf("bytewalk: %d %d\n",
					 (void *)b == (void *)&u,
					 (b[0] + b[1] + b[2] + b[3]) == 10);
	}
}
