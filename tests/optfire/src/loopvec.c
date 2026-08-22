extern int printf(const char *, ...);

float va[8], vb[8], vadd8[8], vsub8[8];
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
	loop_add();
	loop_sub();
	loop_dmul();
	for (i = 0; i < 8; i++)
		printf("%g ", vadd8[i]);
	printf("| ");
	for (i = 0; i < 8; i++)
		printf("%g ", vsub8[i]);
	printf("| ");
	for (i = 0; i < 4; i++)
		printf("%g ", wmul4[i]);
	printf("\n");
	return 0;
}
