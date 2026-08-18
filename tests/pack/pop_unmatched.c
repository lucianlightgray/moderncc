/* T-mac-30130 (slice): an unmatched #pragma pack(pop) warns and continues
 * (matching gcc/clang + mcc's own #pragma options align=reset), not a hard error.
 * Normal push/pop nesting still yields correct packed/unpacked layout. */
#pragma pack(pop)
#pragma pack(push, 1)
struct P { char c; int i; };
#pragma pack(pop)
struct U { char c; int i; };
_Static_assert(sizeof(struct P) == 5, "packed");
_Static_assert(sizeof(struct U) == 8, "unpacked");
int main(void) { return 0; }
