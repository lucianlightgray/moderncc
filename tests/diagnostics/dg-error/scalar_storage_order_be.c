/* dg-error: unsupported member */
/* A reversed-scalar-storage-order struct with a scalar-float member that has no
 * same-width integer carrier for the byte-swap is refused on every target that
 * has such a type.  Which type that is differs by target, so the member is
 * selected per target: the SysV x86 family carries an 80-bit x87 `long double`
 * (VT_LDOUBLE, no 128-bit carrier), while arm64/riscv64 fold `long double` down
 * to `double` (swappable) and instead expose `__float128`.  The Windows x64 ABI
 * has neither leaf (`long double` == `double`, and `__float128` is not a target
 * type there), so the whole cell is skipped on WIN32 in CMakeLists.txt.  This
 * replaced earlier shapes as each became implemented — short/char/bool
 * bit-fields, half-float scalars, and packed unit-spanning bit-fields are all
 * supported now (T-lin-10394, verified byte-equal to gcc-16 in
 * tests/exec/types/sso.c); the still-unimplemented leaves are __float128 and
 * 80-bit x87 long double. */
struct __attribute__((scalar_storage_order("big-endian"))) be {
#if defined(__x86_64__) || defined(__i386__)
	long double v;
#else
	__float128 v;
#endif
};

int f(void)
{
	struct be s;
	s.v = 1;
	return (int)s.v;
}
