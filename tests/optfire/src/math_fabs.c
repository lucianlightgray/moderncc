double fabs(double);
float fabsf(float);

double f(double a, double b)
{
	return fabs(a) + fabs(a * b);
}

float g(float a)
{
	return fabsf(a) * 0.5f;
}

int main(void) { return f(-2.0, -3.0) + (double)g(-1.0f) != 8.5; }
