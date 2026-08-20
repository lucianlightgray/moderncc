extern int printf(const char *, ...);

static double dcse(double a, double b)
{
	double x = a * b - a;
	double y = a * b - a;
	return x + y;
}

static float scse(float a, float b)
{
	float x = a * b - a;
	float y = a * b - a;
	return x + y;
}

static int icse(int a, int b)
{
	int x = a * b - a;
	int y = a * b - a;
	return x + y;
}

int main(void)
{
	printf("%.1f %.1f %d\n", dcse(3, 4), (double)scse(3, 4), icse(3, 4));
	return 0;
}
