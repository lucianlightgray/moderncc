/* Type-incompleteness: the outer cast to `struct S *` changes the static type
   but emits no machine code, so it leaves no trace in the arena.  The `->`
   then sees the inner cast's integer type and the region is not
   type-complete.  Same shape as tests/rir/gap/abort.c. */
struct S {
	int v;
};

int f(unsigned long long *p, int i) {
	return ((struct S *)(unsigned long)p[i])->v;
}
