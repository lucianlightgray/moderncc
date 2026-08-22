extern int printf(const char *, ...);

float va[8], vb[8], vadd8[8], vsub8[8];
float ra[10], rb[10], radd10[10];
float ua[13], ub[13], udiv[13];
double wa[4], wb[4], wmul4[4];

__attribute__((noinline)) static void loop_add(void)
{
	int i;
	for (i = 0; i < 8; i++)
		vadd8[i] = va[i] + vb[i];
}

__attribute__((noinline)) static void loop_sub(void)
{
	int i;
	for (i = 0; i < 8; i++)
		vsub8[i] = va[i] - vb[i];
}

__attribute__((noinline)) static void loop_dmul(void)
{
	int i;
	for (i = 0; i < 4; i++)
		wmul4[i] = wa[i] * wb[i];
}

__attribute__((noinline)) static void loop_add10(void)
{
	int i;
	for (i = 0; i < 10; i++)
		radd10[i] = ra[i] + rb[i];
}

__attribute__((noinline)) static void loop_rt(int n)
{
	int i;
	for (i = 0; i < n; i++)
		udiv[i] = ua[i] / ub[i];
}

int main(int argc, char **argv)
{
	int i;
	(void)argv;
	for (i = 0; i < 8; i++) {
		va[i] = (float)(argc + i);
		vb[i] = (float)(i + 1);
	}
	for (i = 0; i < 4; i++) {
		wa[i] = (double)(argc + i + 1);
		wb[i] = 3.0;
	}
	for (i = 0; i < 10; i++) {
		ra[i] = (float)(argc + i);
		rb[i] = (float)(2 * i);
	}
	for (i = 0; i < 13; i++) {
		ua[i] = (float)((argc + i) * 6);
		ub[i] = (float)(i + 2);
	}
	loop_add();
	loop_sub();
	loop_dmul();
	loop_add10();
	loop_rt(argc + 12);
	for (i = 0; i < 8; i++)
		printf("%g ", vadd8[i]);
	printf("| ");
	for (i = 0; i < 8; i++)
		printf("%g ", vsub8[i]);
	printf("| ");
	for (i = 0; i < 4; i++)
		printf("%g ", wmul4[i]);
	printf("| ");
	for (i = 0; i < 10; i++)
		printf("%g ", radd10[i]);
	printf("| ");
	for (i = 0; i < argc + 12; i++)
		printf("%g ", udiv[i]);
	printf("\n");
	return 0;
}
