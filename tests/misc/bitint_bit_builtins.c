int main(void) {
	unsigned _BitInt(40) a = 0xFF;
	unsigned _BitInt(100) b = (unsigned _BitInt(100))1 << 70;
	unsigned _BitInt(33) c = 1;
	unsigned _BitInt(128) d = (unsigned _BitInt(128))1 << 100;
	unsigned _BitInt(40) z = 0;
	unsigned int u = 0xFF;
	unsigned long long ull = 0xFF;

	if (__builtin_clzg(a) != 32) return 1;
	if (__builtin_ctzg(a) != 0) return 2;
	if (__builtin_popcountg(a) != 8) return 3;
	if (__builtin_clzg(b) != 29) return 4;
	if (__builtin_popcountg(b) != 1) return 5;
	if (__builtin_clzg(c) != 32) return 6;
	if (__builtin_clzg(d) != 27) return 7;
	if (__builtin_clzg(z, 99) != 99) return 8;
	if (__builtin_ctzg(z, 99) != 99) return 9;

	if (__builtin_clzg(u) != 24) return 10;
	if (__builtin_clzg(ull) != 56) return 11;
	if (__builtin_popcountg(ull) != 8) return 12;

	if (__builtin_bitprecisionof(a) != 40) return 13;
	if (__builtin_bitprecisionof(b) != 100) return 14;
	if (__builtin_bitprecisionof(c) != 33) return 15;
	if (__builtin_bitprecisionof(d) != 128) return 16;
	if (__builtin_bitprecisionof(u) != 32) return 17;
	if (__builtin_bitprecisionof(ull) != 64) return 18;

	return 0;
}
