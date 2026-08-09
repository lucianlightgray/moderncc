static int mixw(int a) { return a < 4ULL; }

static int mixs(int a, unsigned int b) { return a * 2 + 4 - b > 0; }

static long long mixu(unsigned int a, unsigned long long b) { return a - b; }

static int narrow(unsigned char a, unsigned char b) { return a - b; }

static int narrows(unsigned short a, unsigned short b) { return a * b; }

static unsigned int tern(unsigned int a, unsigned int b) {
	return a == b ? 0 : b;
}

static long long shifted(int a, unsigned long long n) { return a << n; }

int main(void) {
	if ((int)(mixw(-3)) != 0)
		return 1;
	if ((int)(mixw(3)) != 1)
		return 2;
	if ((int)(mixw(1000)) != 0)
		return 3;
	if ((int)(mixs(-100, 7u)) != 1)
		return 4;
	if ((int)(mixs(1, 9u)) != 1)
		return 5;
	if ((long long)(mixu(4u, 32ULL)) != -28LL)
		return 6;
	if ((long long)(mixu(0u, 1ULL)) != -1LL)
		return 7;
	if ((int)(narrow(3, 200)) != -197)
		return 8;
	if ((int)(narrow(200, 3)) != 197)
		return 9;
	if ((int)(narrows(300, 300)) != 90000)
		return 10;
	if ((unsigned int)(tern(1u, 7u)) != 7u)
		return 11;
	if ((unsigned int)(tern(5u, 5u)) != 0u)
		return 12;
	if ((unsigned int)(tern(1u, -3u)) != -3u)
		return 13;
	if ((long long)(shifted(3, 4ULL)) != 48LL)
		return 14;
	if ((long long)(shifted(-1, 1ULL)) != -2LL)
		return 15;
	return 0;
}
