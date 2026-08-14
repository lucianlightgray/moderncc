/* dg-error: scalar_storage_order */
/* D6, the board's "most dangerous open item": mcc objects link against gcc's,
 * so an ABI attribute that mcc ignores is a silent disagreement about memory.
 *
 * Measured 2026-08-14: for this struct holding 0x01020304, gcc writes the bytes
 * 01 02 03 04 and mcc writes 04 03 02 01. Reading `v` back through the struct
 * gives 0x01020304 under BOTH, so a test that does not inspect the raw bytes
 * cannot see the difference at all -- which is what made it dangerous.
 *
 * It was a `warning: attribute ignored`, and -w silences that. -w is what most
 * builds pass, including this tree's own harness. An attribute that changes the
 * ABI has to be refused, not ignored. Asking for little-endian on a
 * little-endian target is a real no-op and is still accepted in silence. */
struct __attribute__((scalar_storage_order("big-endian"))) be {
	unsigned int v;
};

int f(void)
{
	struct be s;
	s.v = 1;
	return (int)s.v;
}
