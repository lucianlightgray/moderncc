extern int printf(const char *, ...);

static int zeros_a[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int zeros_b[8] = {0};
static long zeros_c[4] = {0, 0, 0, 0};

int main(void)
{
	int i;
	long sum = 0;

	for (i = 0; i < 16; i++)
		sum += zeros_a[i];
	for (i = 0; i < 8; i++)
		sum += zeros_b[i];
	for (i = 0; i < 4; i++)
		sum += zeros_c[i];

	zeros_a[3] = 11;
	zeros_b[2] = 22;
	zeros_c[1] = 33;

	sum += zeros_a[3] + zeros_b[2] + (int)zeros_c[1];
	printf("zero_bss=%ld\n", sum);
	return 0;
}
