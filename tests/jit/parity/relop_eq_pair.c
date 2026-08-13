extern int printf(const char *, ...);
static int ieq(int x, int y){ return (x > 0) == (y > 0); }
static int ine(int x, int y){ return (x < 0) != (y < 0); }
static int ueq(unsigned a, unsigned b){ return (b == 0) | (a < b); }
static int uand(unsigned a, unsigned b){ return (b != 0) & (a >= b); }
static int feq(float *d){ return (d[0] > 0) == (d[1] > 0); }
static int fxor(double *d){ return (d[0] < 0) ^ (d[1] < 0); }
int main(void){
	int i, j, t = 0;
	float f[2];
	double g[2];
	for (i = -3; i <= 3; i++)
		for (j = -3; j <= 3; j++) {
			f[0] = (float)i; f[1] = (float)j;
			g[0] = (double)i; g[1] = (double)j;
			t = t * 31 + ieq(i, j) + ine(i, j) * 3 + ueq((unsigned)i, (unsigned)j) * 5 +
					uand((unsigned)i, (unsigned)j) * 7 + feq(f) * 11 + fxor(g) * 13;
		}
	printf("%d\n", t);
	return 0;
}
