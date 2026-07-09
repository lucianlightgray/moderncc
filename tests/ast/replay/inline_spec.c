static int choose(int flag, int a, int b) {
	if (flag)
		return a + 1;
	else
		return b + 2;
}
static int clampk(int x, int k) {
	if (k > 0)
		return x + k;
	return x - k;
}
static int mul(int x, int y) {
	return x * y;
}
static int addk(int x, int k) {
	return x + k;
}

int main(void) {
	int p = choose(1, 10, 99);
	int q = choose(0, 99, 20);
	int c = clampk(5, 3);
	int m = mul(4, 6);
	int v = 10;
	int r = addk(v, 5);
	return p + q + c + m + r - 38;
}
