int main(void) {
	double a = 2.5;
	double b = a * 4.0 + 1.0;
	float f = 3.0f;
	int n = (int)(b + f);
	if (b > 10.0)
		n += 1;
	return n;
}
