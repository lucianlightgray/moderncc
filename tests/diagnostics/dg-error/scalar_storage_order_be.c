/* dg-error: spanning its storage unit */
/* A reversed-scalar-storage-order bit-field that spans its storage unit is
 * refused on every target (a pure-codegen refusal, unlike `long double`, which
 * is 8-byte == double on arm64 and the MSVC/PE target and so COMPILES there).
 * Short/char bit-fields are now implemented (T-lin-10394, verified byte-equal to
 * gcc-16 in tests/exec/types/sso.c); the still-unimplemented case is a field
 * that crosses its unit boundary, which the load/store swap+flip cannot place. */
#pragma pack(1)
struct __attribute__((scalar_storage_order("big-endian"))) be {
	unsigned char pad : 4;
	unsigned int v : 30;
};

int f(void)
{
	struct be s;
	s.v = 1;
	return (int)s.v;
}
