static int sum(int n) {
	int a[n];
	int i, s = 0;
	for (i = 0; i < n; i++)
		a[i] = i + 1;
	{
		int b[n];
		for (i = 0; i < n; i++)
			b[i] = a[i] * 2;
		for (i = 0; i < n; i++)
			s += b[i];
	}
	for (i = 0; i < n; i++)
		s += a[i];
	return s;
}

int main(void) {
	return sum(8) - 66;
}
