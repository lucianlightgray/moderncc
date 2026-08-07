double sqrt(double);

double f(double a, double b)
{
	double u = a * a + b * b;
	return sqrt(u);
}

int main(void) { return f(3.0, 4.0) != 5.0; }
