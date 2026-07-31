/* Minimal reproducer for Darwin TLV linking, referenced by the "Implement Darwin
 * TLV linking so --embed-jit works on arm64-darwin" item in docs/TODO.md.
 *
 * Compile the TLS half with a system compiler and link it with mcc:
 *
 *     clang -c -O1 -DTLV_LIB tests/macho/tlv_extern.c -o lib.o
 *     mcc   -O1        tests/macho/tlv_extern.c lib.o -o prog
 *     ./prog                      # want: tv=42 then tv=7
 *
 * The clang object carries exactly two ARM64_RELOC_TLVP_LOAD_PAGE21 (type 8) and
 * two ARM64_RELOC_TLVP_LOAD_PAGEOFF12 (type 9) relocations -- check with
 * `otool -r lib.o`, column 5 is the type. mcc rejects those today:
 *
 *     Mach-O: unsupported arm64 relocation type 9 (TLVP and POINTER_TO_GOT
 *     are not implemented)
 *
 * Mapping types 8/9 onto R_AARCH64_ADR_GOT_PAGE / R_AARCH64_LD64_GOT_LO12_NC
 * makes the link succeed but the program SIGABRTs before main, because mcc also
 * drops the incoming __thread_vars section and never sets
 * MH_HAS_TLV_DESCRIPTORS. See the TODO item for the other three parts. This file
 * is deliberately not wired into ctest: it documents a known-failing shape, and
 * a cell that asserts the current failure would have to be rewritten by whoever
 * fixes it. Run it by hand from the recipe above.
 */

#ifdef TLV_LIB

_Thread_local int tv = 42;

int get_tv(void) { return tv; }
void set_tv(int x) { tv = x; }

#else

extern int printf(const char *, ...);
int get_tv(void);
void set_tv(int);

int main(void)
{
	printf("tv=%d\n", get_tv());
	set_tv(7);
	printf("tv=%d\n", get_tv());
	return 0;
}

#endif
