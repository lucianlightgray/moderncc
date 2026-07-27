extern int printf(const char *, ...);

double sqrt(double);
double fabs(double);

double gv = -6.25;

int main(void)
{
	double a = sqrt(4.0);
	double b = fabs(gv);
	printf("%.4f %.4f\n", a, b);
	return 0;
}
