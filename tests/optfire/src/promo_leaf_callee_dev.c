long leaf(long a, long b, long c, long d)
{
	long p = a + 1, q = b + 2, r = c + 3, s = d + 4;
	long t = a ^ b, u = b ^ c, v = c ^ d, w = d ^ a;
	long acc = 0;
	int i;
	for (i = 0; i < 4; i++) {
		p += t;
		q += u;
		r += v;
		s += w;
		t ^= p;
		u ^= q;
		v ^= r;
		w ^= s;
		acc += p + q + r + s + t + u + v + w;
	}
	return acc;
}

int main(void) { return leaf(1, 2, 3, 4) != 0; }
