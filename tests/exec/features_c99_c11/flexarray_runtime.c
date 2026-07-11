#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

struct vec {
	size_t n;
	int data[];
};
struct buf {
	size_t n;
	char data[];
};
typedef struct {
	int k;
	long v[];
} tdef_fam;

struct aligned_fam {
	char tag;
	double d[];
};

union with_fam {
	int i;
	struct {
		int len;
		char s[];
	} str;
};

int main(void) {
	int ok = 1;

	ok &= (sizeof(struct vec) == offsetof(struct vec, data));
	ok &= (sizeof(struct buf) == offsetof(struct buf, data));
	ok &= (sizeof(tdef_fam) == offsetof(tdef_fam, v));

	ok &= (sizeof(struct aligned_fam) == offsetof(struct aligned_fam, d));
	ok &= (_Alignof(struct aligned_fam) == _Alignof(double));
	ok &= (offsetof(struct aligned_fam, d) == sizeof(double));

	struct vec *p = malloc(sizeof *p + 5 * sizeof(int));
	p->n = 5;
	for (int i = 0; i < 5; i++)
		p->data[i] = i * i;
	long sum = 0;
	for (size_t i = 0; i < p->n; i++)
		sum += p->data[i];
	ok &= (sum == 0 + 1 + 4 + 9 + 16);

	struct vec header = *p;
	ok &= (header.n == p->n);
	ok &= (sizeof header == offsetof(struct vec, data));
	free(p);

	struct buf *b = malloc(sizeof *b + 8);
	b->n = 8;
	strcpy(b->data, "abc");
	ok &= (strcmp(b->data, "abc") == 0);
	free(b);

	tdef_fam *t = malloc(sizeof *t + 3 * sizeof(long));
	t->k = 3;
	t->v[0] = 10;
	t->v[1] = 20;
	t->v[2] = 30;
	ok &= (t->v[0] + t->v[1] + t->v[2] == 60);
	free(t);

	struct aligned_fam *a = malloc(sizeof *a + 4 * sizeof(double));
	a->tag = 'z';
	for (int i = 0; i < 4; i++)
		a->d[i] = i + 0.5;
	ok &= (a->tag == 'z');
	ok &= (a->d[3] == 3.5);
	ok &= (((uintptr_t)&a->d[0] % _Alignof(double)) == 0);
	free(a);

	union with_fam *u = malloc(sizeof *u + 6);
	u->str.len = 5;
	strcpy(u->str.s, "hello");
	ok &= (u->str.len == 5);
	ok &= (strcmp(u->str.s, "hello") == 0);
	free(u);

	size_t k;
	struct vec *g = malloc(sizeof *g + 3 * sizeof(int));
	g->n = 3;
	int *pd = g->data;
	for (k = 0; k < g->n; k++)
		pd[k] = (int)(100 + k);
	ok &= (g->data[0] == 100 && g->data[2] == 102);
	free(g);

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
