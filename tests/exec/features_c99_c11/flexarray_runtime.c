/* Flexible array members (C11 §6.7.2.1p18). Runtime behavior: sizeof excludes
 * the flexible member (== offsetof), heap allocation with a trailing array,
 * read/write, char-FAM string storage, and a typedef'd FAM struct. Mirrors the
 * valid runtime uses in gcc c99-flex-array-*.c (the invalid ones are diagnostic
 * tests, tracked separately). Prints OK. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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

int main(void) {
	int ok = 1;

	/* sizeof excludes the flexible member */
	ok &= (sizeof(struct vec) == offsetof(struct vec, data));
	ok &= (sizeof(struct buf) == offsetof(struct buf, data));
	ok &= (sizeof(tdef_fam) == offsetof(tdef_fam, v));

	/* heap alloc + element access */
	struct vec *p = malloc(sizeof *p + 5 * sizeof(int));
	p->n = 5;
	for (int i = 0; i < 5; i++)
		p->data[i] = i * i;
	long sum = 0;
	for (size_t i = 0; i < p->n; i++)
		sum += p->data[i];
	ok &= (sum == 0 + 1 + 4 + 9 + 16);
	free(p);

	/* char FAM used as a string buffer */
	struct buf *b = malloc(sizeof *b + 8);
	b->n = 8;
	strcpy(b->data, "abc");
	ok &= (strcmp(b->data, "abc") == 0);
	free(b);

	/* typedef'd FAM */
	tdef_fam *t = malloc(sizeof *t + 3 * sizeof(long));
	t->k = 3;
	t->v[0] = 10;
	t->v[1] = 20;
	t->v[2] = 30;
	ok &= (t->v[0] + t->v[1] + t->v[2] == 60);
	free(t);

	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
