double f(double a)
{
	double x = __builtin_fabs(a);
	double y = x * x;
	double z = a * a;
	return __builtin_sqrt(x) + __builtin_sqrt(y) + __builtin_sqrt(z);
}

double g(double a, double b)
{
	double u = a * a + b * b;
	return __builtin_sqrt(u);
}

int main(void) { return f(-4.0) + g(3.0, 4.0) != 0.0; }
