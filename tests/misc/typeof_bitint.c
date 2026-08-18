int main(void) {
	unsigned _BitInt(40) f = 100;
	__typeof__(f) g = f;
	if ((unsigned long long)g != 100) return 1;

	typeof(f) h = f + 1;
	if ((unsigned long long)h != 101) return 2;

	signed _BitInt(33) s = -5;
	__typeof__(s) s2 = s;
	if ((long long)s2 != -5) return 3;

	__typeof__(f) big = ((__typeof__(f))1 << 39);
	big = big + big;
	if ((unsigned long long)big != 0) return 4;

	typeof_unqual(f) u = f * 2;
	if ((unsigned long long)u != 200) return 5;

	_BitInt(100) w = 7;
	__typeof__(w) w2 = w * w;
	if ((long long)w2 != 49) return 6;

	unsigned _BitInt(40) arr[2] = {1, 2};
	__typeof__(arr[0]) e = arr[1];
	if ((unsigned long long)e != 2) return 7;

	return 0;
}
