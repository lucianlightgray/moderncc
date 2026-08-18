#define TID(x) _Generic((x), \
	_BitInt(33): 33, unsigned _BitInt(33): 133, \
	_BitInt(40): 40, unsigned _BitInt(40): 140, \
	_BitInt(63): 63, \
	_BitInt(64): 64, unsigned _BitInt(64): 164, \
	int: 1, unsigned: 101, long long: 2, unsigned long long: 102, \
	default: 0)

int main(void) {
	_BitInt(40) a = 3, b = 5;
	_BitInt(33) c = 5;
	_BitInt(63) d = 7;
	_BitInt(64) e = 9;
	unsigned _BitInt(64) f = 3;
	unsigned _BitInt(40) g = 4;

	if (TID(a + b) != 40) return 1;
	if (TID(a - b) != 40) return 2;
	if (TID(a * b) != 40) return 3;
	if (TID(a << 1) != 40) return 4;
	if (TID(c + c) != 33) return 5;
	if (TID(d * d) != 63) return 6;
	if (TID(e + e) != 64) return 7;
	if (TID(f + f) != 164) return 8;
	if (TID(g + g) != 140) return 9;

	if (TID((int)3 + (int)4) != 1) return 10;
	if (TID((long long)3 + (long long)4) != 2) return 11;

	_BitInt(40) big = ((_BitInt(40))1 << 39);
	_BitInt(40) w = big + big;
	if ((long long)w != 0) return 12;

	unsigned _BitInt(33) u = 0;
	u = u - 1;
	if ((unsigned long long)u != 8589934591ULL) return 13;

	_BitInt(40) s = -1;
	s = s * s;
	if ((long long)s != 1) return 14;

	return 0;
}
