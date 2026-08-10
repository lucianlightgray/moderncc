#include <stdio.h>

#ifndef LABELADDR_KP
#define LABELADDR_KP 0
#endif

int sink;

static int fn_scope_label(int n) {
	int a = n + 1;
	int b = a * 2;
	int c = a * 2;
	int d = b + c;
	int e = d + a * 2;
	int f = e + b - c;
	sink = f;
	goto *&&fdone;
	sink = -1;
fdone:;
	return sink;
}

static void block_scope_label(int n) {
	{
		__label__ bdone;
		int a = n + 3;
		int b = a * 4;
		int c = a * 4;
		int d = b + c;
		int e = d + a * 4;
		int f = e + b - c;
		sink = f;
		goto *&&bdone;
		sink = -1;
	bdone:;
		sink += 0;
	}
}

static int jump_table(int n) {
	static void *tab[4] = {&&l0, &&l1, &&l2, &&l3};
	int a = n + 1;
	int b = a * 2;
	int c = a * 2;
	int acc = b + c + a * 2;
	int r = -1;
	if (n < 0 || n > 3)
		return -1;
	goto *tab[n];
l0:;
	r = acc + 0;
	goto out;
l1:;
	r = acc + 10;
	goto out;
l2:;
	r = acc + 20;
	goto out;
l3:;
	r = acc + 30;
out:;
	return r;
}

int main(void) {
	int bad = 0;
	int v;

	v = fn_scope_label(1);
	printf("fn_scope_label=%d\n", v);
	if (v != 12)
		bad++;

	block_scope_label(1);
	v = sink;
	printf("block_scope_label=%d\n", v);
	if (v != 48)
		bad++;

	for (int i = 0; i < 4; i++) {
		v = jump_table(i);
		printf("jump_table[%d]=%d\n", i, v);
		if (v != (i + 1) * 6 + i * 10)
			bad++;
	}

	if (LABELADDR_KP)
		printf("known-positive skew\n");

	printf("bad=%d\n", bad);
	return bad ? 3 : 0;
}
