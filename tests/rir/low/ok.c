/* Every body here must be wholly lowerable: pure scalar/pointer compute,
   no call, no asm, no global, no address-of-local, and every dereference's
   pointer type survives into the arena. */

int add(int a, int b) { return a + b; }

int loopsum(int *v, int n) {
	int s = 0;
	for (int i = 0; i < n; i++)
		s += v[i];
	return s;
}

void saxpy(int n, float a, float *x, float *y) {
	for (int i = 0; i < n; i++)
		y[i] = a * x[i] + y[i];
}

unsigned mix(unsigned h, unsigned k) {
	h ^= k;
	h *= 0x01000193u;
	return (h << 13) | (h >> 19);
}
