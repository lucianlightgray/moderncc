/* Flexible array members (C11 §6.7.2.1p18). Runtime behavior: sizeof excludes
 * the flexible member, heap allocation with a trailing array, read/write,
 * char-FAM string storage, and a typedef'd FAM struct. Mirrors the valid
 * runtime uses in gcc c99-flex-array-*.c (the invalid ones are diagnostic tests,
 * covered by tests/cli c99_fam_not_last). Prints OK.
 *
 * Deliberately avoids <stddef.h>/offsetof: on the musl-arm sysroot, including
 * stddef.h after the C library headers triggers an incompatible wchar_t
 * redefinition, so the member offset is computed by pointer difference instead.
 * size_t comes from <stdlib.h>. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	/* sizeof excludes the flexible member: it equals the member's offset. */
	struct vec vhdr;
	struct buf bhdr;
	tdef_fam thdr;
	ok &= (sizeof(struct vec) == (size_t)((char *)&vhdr.data - (char *)&vhdr));
	ok &= (sizeof(struct buf) == (size_t)((char *)&bhdr.data - (char *)&bhdr));
	ok &= (sizeof(tdef_fam) == (size_t)((char *)&thdr.v - (char *)&thdr));

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
