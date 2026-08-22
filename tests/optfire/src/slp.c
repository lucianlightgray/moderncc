extern int printf(const char *, ...);

float ga[4], gb[4], gadd[4], gsub[4];
double da[2], db[2], dmul[2];

__attribute__((noinline)) static void vadd(void)
{
	gadd[0] = ga[0] + gb[0];
	gadd[1] = ga[1] + gb[1];
	gadd[2] = ga[2] + gb[2];
	gadd[3] = ga[3] + gb[3];
}

__attribute__((noinline)) static void vsub(void)
{
	gsub[0] = ga[0] - gb[0];
	gsub[1] = ga[1] - gb[1];
	gsub[2] = ga[2] - gb[2];
	gsub[3] = ga[3] - gb[3];
}

__attribute__((noinline)) static void vdmul(void)
{
	dmul[0] = da[0] * db[0];
	dmul[1] = da[1] * db[1];
}

int main(int argc, char **argv)
{
	int i;
	(void)argv;
	for (i = 0; i < 4; i++) {
		ga[i] = (float)(argc + i);
		gb[i] = (float)(i + 1);
	}
	for (i = 0; i < 2; i++) {
		da[i] = (double)(argc + i + 1);
		db[i] = 3.0;
	}
	vadd();
	vsub();
	vdmul();
	printf("%g %g %g %g | %g %g %g %g | %g %g\n", gadd[0], gadd[1], gadd[2],
				 gadd[3], gsub[0], gsub[1], gsub[2], gsub[3], dmul[0], dmul[1]);
	return 0;
}
