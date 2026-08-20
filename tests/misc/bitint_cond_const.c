int main(void) {
	volatile int t = 1, f = 0;

	unsigned _BitInt(200) a = t ? (unsigned _BitInt(200))123 : (unsigned _BitInt(200))456;
	if ((int)a != 123) return 1;
	unsigned _BitInt(200) b = f ? (unsigned _BitInt(200))123 : (unsigned _BitInt(200))456;
	if ((int)b != 456) return 2;

	unsigned _BitInt(200) v = 5;
	unsigned _BitInt(200) c = t ? (unsigned _BitInt(200))7 : v;
	if ((int)c != 7) return 3;
	unsigned _BitInt(200) d = f ? (unsigned _BitInt(200))7 : v;
	if ((int)d != 5) return 4;
	unsigned _BitInt(200) e = t ? v : (unsigned _BitInt(200))9;
	if ((int)e != 5) return 5;
	unsigned _BitInt(200) g = f ? v : (unsigned _BitInt(200))9;
	if ((int)g != 9) return 6;

	unsigned _BitInt(100) sm = f ? (unsigned _BitInt(100))7 : (unsigned _BitInt(100))9;
	if ((int)sm != 9) return 7;
	unsigned _BitInt(300) w = f ? (unsigned _BitInt(300))11 : (unsigned _BitInt(300))22;
	if ((int)w != 22) return 8;

	unsigned _BitInt(200) hi = t ? ((unsigned _BitInt(200))1 << 170) : (unsigned _BitInt(200))0;
	if (hi != ((unsigned _BitInt(200))1 << 170)) return 9;
	unsigned _BitInt(200) zt = f ? ((unsigned _BitInt(200))1 << 170) : (unsigned _BitInt(200))0;
	if (zt != 0) return 10;

	unsigned _BitInt(200) x = 0;
	for (int i = 0; i < 3; i++)
		x += (i < 2) ? (unsigned _BitInt(200))10 : (unsigned _BitInt(200))100;
	if ((int)x != 120) return 11;

	return 0;
}
