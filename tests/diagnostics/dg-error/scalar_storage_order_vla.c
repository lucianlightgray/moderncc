/* dg-error: variable-length array member */
/* A variably-modified struct is laid out at run time and never reaches the
 * reverse-scalar-storage-order member check, so a reversed VLA member would
 * silently store its elements little-endian.  mcc refuses it honestly rather
 * than miscompile (T-lin-10394 sub-item 5).  Fixed-size arrays of a supported
 * element type ARE reversed correctly (see tests/exec/types/sso.c). */
int f(int n)
{
	struct __attribute__((scalar_storage_order("big-endian"))) be {
		int v[n];
	} s;
	s.v[0] = 1;
	return s.v[0];
}
