int main(void) {
	int a[4] = {10, 20, 3, 4};
	int s = 0;
	for (int i = 0; i < 4; i++)
		s += a[i];
	return s + 5;
}
