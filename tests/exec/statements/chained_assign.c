/* Chained assignment `a = b = v`, which the AST recorder models by leaving the
   inner assignment's value as the expression result. Two defects lived here:
   the shared value node was reparented (a non-tree AST), and the outer store
   reads the value from the register the inner store left it in -- which a
   PROMOTED inner target never writes, so the result was a wrong starting value
   (a constant +512 offset on the reduction below). The reduction form is the
   one that matters: plb spectral-norm writes `for (sum = j = 0; ...)`. Integer
   output where possible so the golden is exact. */
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
	return 0;
}
