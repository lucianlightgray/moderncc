/* Type-incompleteness: values whose static type has no portable scalar
   meaning for a shader.  ast_bad_type() rejects bitfields (a width and an
   offset inside a storage unit, not a value) and long double (an x87 80-bit
   format with no GPU equivalent).

   Until 2026-08-06 this fixture instead used a cast chain,
   `((struct S *)(unsigned long)p[i])->v`, whose outer cast changed the static
   type but emitted no machine code and so left no trace in the arena.  That
   defect is fixed: rir_hook_cast_type() now records the post-cast CType, so
   that shape is type-complete and lowerable.  See tests/rir/gap/abort.c. */
struct B {
	unsigned a : 3;
	unsigned b : 5;
};

int f(struct B *p) {
	return (int)p->a + (int)p->b;
}

long double g(long double x, long double y) {
	return x * y + x;
}
