







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

struct rot_s {
	int x, y;
};

struct rot_t {
	struct rot_s m;
	int k;
};

static int rot_bump(int v) {
	return v + 1;
}

static int rot_member_chain(int s) {
	struct rot_s r;
	struct rot_s *q = &r;
	int a = 1, c = 3;
	q->x = c = a = s + a;
	return a + c + q->x;
}

static int rot_deep_member(int s) {
	struct rot_t t;
	struct rot_t *u = &t;
	int a = 2, b = 3, c = 5;
	t.k = 0;
	u->m.x = c = u->m.y = b = a = s * 2 + 1;
	t.k = b = a = t.k + s;
	return a + b + c + t.m.x + t.m.y + t.k;
}

static int rot_array_chain(int s) {
	int arr[4];
	int *p = &arr[1];
	int a = 1, b = 2;
	arr[0] = arr[1] = arr[2] = arr[3] = 0;
	arr[s & 3] = b = a = s + 4;
	p[1] = a = b + 1;
	return arr[0] + arr[1] * 3 + arr[2] * 5 + arr[3] * 7 + a + b;
}

static int rot_const_left(int s) {
	struct rot_s r;
	struct rot_s *q = &r;
	int a = 1, c = 3;
	q->x = c = 2 * (a = s + 7);
	q->y = c = 3 + (a = a * 2);
	return a + c + r.x + r.y;
}

static int rot_call_arg(int s) {
	struct rot_s r;
	struct rot_s *q = &r;
	int a = 1, c = 3;
	q->x = c = a = rot_bump(s) + a;
	q->y = rot_bump(c = a = a + 2);
	return a + c + r.x + r.y;
}

static int rot_member_of_member(int s) {
	struct rot_t t;
	struct rot_t *u = &t;
	int a = 1, b = 2, c = 3;
	t.k = 7;
	u->m.x = t.m.y = c = b = a = s + 3;
	c = u->m.x = b = a = a * 2 - 1;
	return a + b + c + t.m.x + t.m.y + t.k;
}

static int rot_volatile_mid(int s) {
	struct rot_s r;
	struct rot_s *q = &r;
	volatile int c = 3;
	int a = 1;
	r.y = 0;
	q->x = c = a = s + a;
	q->y = c = a = a * 3;
	return a + c + r.x + r.y;
}

static int rot_volatile_elem(int s) {
	int arr[2];
	volatile int c = 3;
	int a = 1;
	arr[0] = 0;
	arr[1] = 0;
	arr[s & 1] = c = a = s + a;
	return a + c + arr[0] + arr[1] * 3;
}

static int rot_narrow_mid(int s) {
	struct rot_s r;
	struct rot_s *q = &r;
	/* signed char, not char: this case is about the narrowing in a chained
	 * assignment, not about plain char's signedness, and plain char is UNSIGNED
	 * on arm, arm64 and riscv64 per their ABIs. With `char` the golden below
	 * silently encoded x86_64's answer and the three qemu -exec cells failed on
	 * a portable program giving a portable result. Signed is what the case
	 * meant, and it is what x86_64 was already testing, so the golden is
	 * unchanged. */
	signed char c = 3;
	short h = 1;
	int a = 1;
	q->x = c = a = s * 100 + 7;
	q->y = h = a = a * 400 + 1;
	return a + c + h + r.x + r.y;
}

static int rot_dead_mid(int s) {
	struct rot_s r;
	struct rot_s *q = &r;
	int a = 1, c = 3, d = 0;
	q->x = d = c = a = s * 3;
	c = a * 2;
	d = 7;
	q->y = c = a = a + d;
	return a + c + d + r.x + r.y;
}

static int rot_impure_target(int s) {
	int arr[4];
	int i = 1;
	int a = 1, c = 3;
	arr[0] = arr[1] = arr[2] = arr[3] = 0;
	arr[i++] = c = a = s + 5;
	return a + c + i + arr[0] + arr[1] * 3 + arr[2] * 5 + arr[3] * 7;
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
	for (i = -2; i < 4; i++)
		printf("rot %d %d %d %d %d %d\n", rot_member_chain(i), rot_deep_member(i),
					 rot_array_chain(i), rot_const_left(i), rot_call_arg(i),
					 rot_member_of_member(i));
	for (i = -2; i < 4; i++)
		printf("rotb %d %d %d %d %d\n", rot_volatile_mid(i), rot_volatile_elem(i),
					 rot_narrow_mid(i), rot_dead_mid(i), rot_impure_target(i));
	return 0;
}
