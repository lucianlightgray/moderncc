static signed _BitInt(128) cs128 = -5;
static signed _BitInt(100) cs100 = -5;
static unsigned _BitInt(128) cu128 =
	((unsigned _BitInt(128))0xdeadbeefULL << 64) | (unsigned _BitInt(128))0x12345678ULL;
static signed _BitInt(40) cs40 = -3;
static unsigned _BitInt(40) cu40 = 0xffULL;

int main(void) {
	__int128 c1 = (__int128)(_BitInt(128)) - 5;
	if (c1 != (__int128)-5) return 1;
	if ((unsigned long long)(unsigned __int128)c1 != 0xFFFFFFFFFFFFFFFBULL) return 2;
	if ((unsigned long long)((unsigned __int128)c1 >> 64) != 0xFFFFFFFFFFFFFFFFULL) return 3;

	__int128 c2 = (__int128)(_BitInt(100)) - 5;
	if (c2 != (__int128)-5) return 4;
	if ((unsigned long long)((unsigned __int128)c2 >> 64) != 0xFFFFFFFFFFFFFFFFULL) return 5;

	__int128 r1 = (__int128)cs128;
	if (r1 != (__int128)-5) return 6;
	if ((unsigned long long)((unsigned __int128)r1 >> 64) != 0xFFFFFFFFFFFFFFFFULL) return 7;

	__int128 r2 = (__int128)cs100;
	if (r2 != (__int128)-5) return 8;
	if ((unsigned long long)((unsigned __int128)r2 >> 64) != 0xFFFFFFFFFFFFFFFFULL) return 9;

	unsigned __int128 u1 = (unsigned __int128)cu128;
	if ((unsigned long long)(u1 >> 64) != 0xdeadbeefULL) return 10;
	if ((unsigned long long)u1 != 0x12345678ULL) return 11;

	__int128 p1 = (__int128)(_BitInt(128))7;
	if (p1 != 7) return 12;

	__int128 n1 = (__int128)cs40;
	if (n1 != (__int128)-3) return 13;
	if ((unsigned long long)((unsigned __int128)n1 >> 64) != 0xFFFFFFFFFFFFFFFFULL) return 14;

	__int128 n2 = (__int128)cu40;
	if (n2 != 0xff) return 15;

	return 0;
}
