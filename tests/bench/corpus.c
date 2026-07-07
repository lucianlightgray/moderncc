#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAP_CAP 1024

typedef struct
{
	unsigned long keys[MAP_CAP];
	long vals[MAP_CAP];
	unsigned char used[MAP_CAP];
	int count;
} map_t;

static unsigned long mix64(unsigned long x) {
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdUL;
	x ^= x >> 33;
	x *= 0xc4ceb9fe1a85ec53UL;
	x ^= x >> 33;
	return x;
}

static void map_init(map_t *m) {
	memset(m, 0, sizeof *m);
}

static int map_put(map_t *m, unsigned long k, long v) {
	unsigned long h = mix64(k) % MAP_CAP;
	int probes = 0;
	while (m->used[h]) {
		if (m->keys[h] == k) {
			m->vals[h] = v;
			return 0;
		}
		h = (h + 1) % MAP_CAP;
		if (++probes >= MAP_CAP)
			return -1;
	}
	m->used[h] = 1;
	m->keys[h] = k;
	m->vals[h] = v;
	m->count++;
	return 1;
}

static long map_get(const map_t *m, unsigned long k, long def) {
	unsigned long h = mix64(k) % MAP_CAP;
	int probes = 0;
	while (m->used[h]) {
		if (m->keys[h] == k)
			return m->vals[h];
		h = (h + 1) % MAP_CAP;
		if (++probes >= MAP_CAP)
			break;
	}
	return def;
}

static long gcd_long(long a, long b) {
	if (a < 0)
		a = -a;
	if (b < 0)
		b = -b;
	while (b) {
		long t = a % b;
		a = b;
		b = t;
	}
	return a;
}

static unsigned long ipow(unsigned long base, unsigned exp) {
	unsigned long r = 1;
	while (exp) {
		if (exp & 1u)
			r *= base;
		base *= base;
		exp >>= 1;
	}
	return r;
}

static int is_prime(unsigned long n) {
	unsigned long i;
	if (n < 2)
		return 0;
	if (n % 2 == 0)
		return n == 2;
	for (i = 3; i * i <= n; i += 2)
		if (n % i == 0)
			return 0;
	return 1;
}

static void isort(long *a, int n) {
	int i, j;
	for (i = 1; i < n; i++) {
		long key = a[i];
		for (j = i - 1; j >= 0 && a[j] > key; j--)
			a[j + 1] = a[j];
		a[j + 1] = key;
	}
}

static long dot(const long *a, const long *b, int n) {
	long s = 0;
	int i;
	for (i = 0; i < n; i++)
		s += a[i] * b[i];
	return s;
}

static unsigned long str_hash(const char *s) {
	unsigned long h = 1469598103934665603UL;
	while (*s) {
		h ^= (unsigned char)*s++;
		h *= 1099511628211UL;
	}
	return h;
}

static int str_rev(char *s) {
	int n = (int)strlen(s), i;
	for (i = 0; i < n / 2; i++) {
		char t = s[i];
		s[i] = s[n - 1 - i];
		s[n - 1 - i] = t;
	}
	return n;
}

static int count_words(const char *s) {
	int in = 0, n = 0;
	for (; *s; s++) {
		if (*s == ' ' || *s == '\t' || *s == '\n')
			in = 0;
		else if (!in) {
			in = 1;
			n++;
		}
	}
	return n;
}

#define GEN_ARITH(N)                                             \
	static long arith_##N(long a, long b, long c) {                \
		long x = a * (N) + b - c;                                    \
		long y = (a ^ b) + (c << ((N) % 13));                        \
		long z = (x > y) ? (x - y) : (y - x);                        \
		return z * (N) + gcd_long(x + (N), y + 1) - (a % ((N) + 1)); \
	}

#define GEN_POLY(N)                                      \
	static long poly_##N(long t) {                         \
		return ((t + (N)) * (t - (N)) + (N) * t) % 1000003L; \
	}

#define GEN10(M, B)               \
	M(B##0)                         \
	M(B##1) M(B##2) M(B##3) M(B##4) \
			M(B##5) M(B##6) M(B##7) M(B##8) M(B##9)

#define GEN100(M, B)                                          \
	GEN10(M, B##0)                                              \
	GEN10(M, B##1) GEN10(M, B##2) GEN10(M, B##3) GEN10(M, B##4) \
			GEN10(M, B##5) GEN10(M, B##6) GEN10(M, B##7) GEN10(M, B##8) GEN10(M, B##9)

GEN100(GEN_ARITH, 1)
GEN100(GEN_POLY, 1)

#define CALL10(F, B)                                     \
	(F##B##0(i, i + 1, i + 2) + F##B##1(i, i + 2, i + 3) + \
	 F##B##2(i, i + 3, i + 4) + F##B##3(i, i + 4, i + 5) + \
	 F##B##4(i, i + 5, i + 6) + F##B##5(i, i + 6, i + 7) + \
	 F##B##6(i, i + 7, i + 8) + F##B##7(i, i + 8, i + 9) + \
	 F##B##8(i, i + 9, i + 10) + F##B##9(i, i + 10, i + 11))

static long sum_arith(long i) {
	return CALL10(arith_1, 0) + CALL10(arith_1, 1) + CALL10(arith_1, 2) +
				 CALL10(arith_1, 3) + CALL10(arith_1, 4) + CALL10(arith_1, 5) +
				 CALL10(arith_1, 6) + CALL10(arith_1, 7) + CALL10(arith_1, 8) +
				 CALL10(arith_1, 9);
}

int main(void) {
	map_t m;
	long a[16], b[16];
	int i;
	long acc = 0;
	char buf[64];

	map_init(&m);
	for (i = 0; i < 16; i++) {
		a[i] = (long)ipow(2, (unsigned)(i % 10)) - i;
		b[i] = poly_100(i) + poly_150(i);
		map_put(&m, (unsigned long)i, a[i]);
	}
	isort(a, 16);
	acc += dot(a, b, 16);
	acc += (long)map_get(&m, 3, -1);
	acc += sum_arith(acc % 97);
	acc += is_prime((unsigned long)(acc < 0 ? -acc : acc));
	strcpy(buf, "modern c compiler benchmark corpus");
	acc += str_rev(buf);
	acc += (long)(str_hash(buf) & 0xffff);
	acc += count_words(buf);
	printf("%ld\n", acc);
	return (int)(acc & 0x7f);
}
