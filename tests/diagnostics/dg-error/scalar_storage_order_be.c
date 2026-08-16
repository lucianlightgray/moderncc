/* dg-error: scalar_storage_order */
/* Reverse scalar_storage_order is IMPLEMENTED for integer scalar members
 * (T-lin-10010) -- `struct __attribute__((scalar_storage_order("big-endian")))
 * { unsigned v; }` now stores v byte-reversed, byte-identical to gcc, and the
 * plain-scalar exec proof lives in tests/exec/types/sso.c.  What slice 1 does
 * NOT implement is the recursive cases; they are refused rather than emitted
 * with the wrong byte order.  This checks the array-member refusal still fires. */
struct __attribute__((scalar_storage_order("big-endian"))) be {
	unsigned int a[4];
};

int f(void)
{
	struct be s;
	s.a[0] = 1;
	return (int)s.a[0];
}
