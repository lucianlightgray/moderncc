int main(void) {
	int a[5];
	int s = 0;
	for (int i = 0; i < 5; i++)
		a[i] = i * 3;
	for (int i = 0; i < 5; i++)
		s += a[i];
	return s + 12;
}
