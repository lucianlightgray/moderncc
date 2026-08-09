







#include <stdio.h>

static double v[64];
static int g[64];

static double reduce_chain(int n) {
	int j;
	double sum;
	for (sum = j = 0; j < n; j++)
		sum += v[j] / (j + 1);
	return sum;
}

static long int_chain(int n) {
	int j;
	long s;
	for (s = j = 0; j < n; j++)
		s += g[j];
	return s;
}

static long triple_chain(int n) {
	int i, j;
	long a, b;
	a = b = i = j = 0;
	for (; i < n; i++) {
		a += i;
		b += a;
		j = i;
	}
	return a + b + j;
}

static long chain_in_expr(int n) {
	int i = 0, k = 0;
	long t = 0;
	while (i < n) {
		t += (k = i + 1);
		i++;
	}
	return t + k;
}

static double mixed_type_chain(int n) {
	int j;
	double d;
	float f;
	for (d = f = j = 0; j < n; j++) {
		d += j * 0.5;
		f += 1.0f;
	}
	return d + (double)f;
}

static long chain_reassign(int n) {
	int i, a, b;
	long t = 0;
	for (i = 0; i < n; i++) {
		a = b = i;
		t += a + b;
		a = b = 0;
		t += a + b;
	}
	return t;
}

static int deep3_chain(int s) {
	int a, b, c;
	c = b = a = s + 1;
	a = b = c = a * 2 + b;
	return a + b + c;
}

static int deep4_chain(int s) {
	int a, b, c, d;
	d = c = b = a = s + 1;
	a = b = c = d = a * 2 + b;
	return a + b + c + d;
}

static int chain_feeds_chain(int s) {
	int a, b, c;
	c = b = a = s + 1;
	a = b = c * 3 + a;
	return a + b + c;
}

static int chain_self_read(int s) {
	int a = 1, b = 2, c = 3, d = 4, e = 5;
	e = b = d = a;
	b = e = a = d = s * d;
	c = c * s;
	b = 5;
	c = e = d = s * d * s;
	a = d = e = b;
	return a + b + c + d + e;
}

static int chain_dead_target(int s) {
	int a = 1, b = 2;
	a = s - 4;
	a = s - s;
	b = a = s * 7 - a;
	return a + b;
}

int main(void) {
	int i;
	for (i = 0; i < 64; i++) {
		v[i] = i * 0.5 + 1.0;
		g[i] = i * 3 - 7;
	}
	printf("reduce %.9f\n", reduce_chain(64));
	printf("int %ld\n", int_chain(64));
	printf("triple %ld\n", triple_chain(32));
	printf("expr %ld\n", chain_in_expr(20));
	printf("mixed %.6f\n", mixed_type_chain(24));
	printf("reassign %ld\n", chain_reassign(20));
	for (i = -2; i < 4; i++)
		printf("deep %d %d %d %d %d\n", deep3_chain(i), deep4_chain(i),
					 chain_feeds_chain(i), chain_self_read(i), chain_dead_target(i));
	return 0;
}
