/* dg-error: unsupported member */
/* A reversed-scalar-storage-order struct with a __float128 member is refused on
 * every target (MCC_HAVE_FLOAT128 is unconditional): its 128-bit value has no
 * same-width integer carrier for the byte-swap, unlike float/double/half.  This
 * replaced earlier shapes as each became implemented — short/char/bool bit-fields,
 * half-float scalars, and packed unit-spanning bit-fields are all supported now
 * (T-lin-10394, verified byte-equal to gcc-16 in tests/exec/types/sso.c); the
 * still-unimplemented leaves are __float128 and 80-bit x87 long double. */
struct __attribute__((scalar_storage_order("big-endian"))) be {
	__float128 v;
};

int f(void)
{
	struct be s;
	s.v = 1;
	return (int)s.v;
}
