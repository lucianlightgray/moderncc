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
#if defined(__SIZEOF_INT128__)
	/* The generic bit builtins carry _BitInt through __mcc_gu_t, which is a
	   128-bit type only where __int128 exists.  These _BitInt(N>64) value
	   checks therefore require a 128-bit carrier; where it is absent (e.g.
	   x86_64-PE, which has no __int128) the *g builtins read only the low 64
	   bits.  The precision (__builtin_bitprecisionof) and the N<=64 cases are
	   unaffected and stay unguarded. */
	if (__builtin_clzg(b) != 29) return 4;
	if (__builtin_popcountg(b) != 1) return 5;
#endif
	if (__builtin_clzg(c) != 32) return 6;
#if defined(__SIZEOF_INT128__)
	if (__builtin_clzg(d) != 27) return 7;
#endif
	if (__builtin_clzg(z, 99) != 99) return 8;
	if (__builtin_ctzg(z, 99) != 99) return 9;

	if (__builtin_clzg(u) != 24) return 10;
	if (__builtin_clzg(ull) != 56) return 11;
	if (__builtin_popcountg(ull) != 8) return 12;

#if defined(__SIZEOF_INT128__)
	unsigned _BitInt(200) w = (unsigned _BitInt(200))1 << 150;
	unsigned _BitInt(256) x = (unsigned _BitInt(256))1 << 200;
	if (__builtin_clzg(w) != 49) return 20;
	if (__builtin_ctzg(w) != 150) return 21;
	if (__builtin_popcountg(w) != 1) return 22;
	if (__builtin_stdc_bit_width(w) != 151) return 23;
	if (__builtin_stdc_leading_zeros(w) != 49) return 24;
	if (__builtin_stdc_count_ones(w) != 1) return 25;
	if (__builtin_clzg(x) != 55) return 26;
	if (__builtin_ctzg(x) != 200) return 27;
	if (__builtin_popcountg(x) != 1) return 28;
#endif

	if (__builtin_bitprecisionof(a) != 40) return 13;
	if (__builtin_bitprecisionof(b) != 100) return 14;
	if (__builtin_bitprecisionof(c) != 33) return 15;
	if (__builtin_bitprecisionof(d) != 128) return 16;
	if (__builtin_bitprecisionof(u) != 32) return 17;
	if (__builtin_bitprecisionof(ull) != 64) return 18;

	return 0;
}
