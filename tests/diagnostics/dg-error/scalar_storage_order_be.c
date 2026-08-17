/* dg-error: scalar_storage_order */
/* A reversed-scalar-storage-order struct with an unsupported member is refused.
 * The member is a short bit-field: unlike `long double` (which is 8-byte ==
 * double on arm64 and the MSVC/PE target, so a reversed double member COMPILES
 * there and the refusal is not target-uniform), a short bit-field is refused on
 * every target by sso_member_supported.  Retargeted from `long double v;` which
 * was a live red on arm64/PE.  (T-lin-10394 owns implementing these edge cases;
 * update this test if the short-bit-field case lands.) */
struct __attribute__((scalar_storage_order("big-endian"))) be {
	unsigned short v : 4;
};

int f(void)
{
	struct be s;
	s.v = 1;
	return (int)s.v;
}
