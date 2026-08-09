int printf(const char *, ...);

double f1(double a) { return a * 1.5 + 0.25; }
double f3(double a, double b, double c) { return a * b - c * 0.5; }
double f6(double a, double b, double c, double d, double e, double f)
{
	return a + b * c - d / (e + 1.0) + f;
}

double w1(double *p) { return f1(p[0]); }
double w3(double *p) { return f3(p[0], p[1], p[2]); }
double w6(double *p) { return f6(p[0], p[1], p[2], p[3], p[4], p[5]); }

int main(void)
{
	double v[6];
	int i;
	for (i = 0; i < 6; i++)
		v[i] = (double)(i + 1) * 1.25 - 3.0;
	printf("f1=%.6f f3=%.6f f6=%.6f\n", w1(v), w3(v), w6(v));
	return 0;
}
