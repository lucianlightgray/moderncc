static int sar(int a, int b) {
	int s = b & 15;
	int t = (a >> s) + (a << (s & 7));
	return t - (a >> 1);
}

static unsigned int shr(unsigned int a, unsigned int b) {
	unsigned int s = b & 15u;
	unsigned int t = (a >> s) | (a << (s & 7u));
	return t ^ (a >> 3);
}

int main(void) {
	if ((int)(sar(-1000, 3)) != -7625)
		return 1;
	if ((int)(sar(123456, 9)) != 185425)
		return 2;
	if ((int)(sar(-7, 0)) != -10)
		return 3;
	if ((unsigned int)(shr(4000000000u, 5u)) != 3535420480u)
		return 4;
	if ((unsigned int)(shr(1u, 31u)) != 128u)
		return 5;
	if ((unsigned int)(shr(0xdeadbeefu, 12u)) != 4043987238u)
		return 6;
	return 0;
}
