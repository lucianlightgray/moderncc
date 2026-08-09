static int sdiv(int a, int b) {
	int q = a / b;
	int r = a % b;
	return q * 3 + r;
}

static unsigned int udiv(unsigned int a, unsigned int b) {
	unsigned int q = a / b;
	unsigned int r = a % b;
	return q + r * 3u;
}

int main(void) {
	if ((int)(sdiv(-100, 7)) != -44)
		return 1;
	if ((int)(sdiv(100, -7)) != -40)
		return 2;
	if ((int)(sdiv(0, 3)) != 0)
		return 3;
	if ((unsigned int)(udiv(4000000000u, 7u)) != 571428580u)
		return 4;
	if ((unsigned int)(udiv(1u, 4000000000u)) != 3u)
		return 5;
	if ((unsigned int)(udiv(123456789u, 1000u)) != 125823u)
		return 6;
	return 0;
}
