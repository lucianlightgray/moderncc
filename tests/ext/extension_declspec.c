/* T-mac-30133 (slice): __extension__ silences -pedantic for the declaration-
 * specifier / struct / enum body it prefixes (long long, enum trailing comma,
 * flexible array member), WITHOUT leaking the suppression to later declarations.
 * (VLA-in-declarator and _Static_assert cases remain — handled after parse_btype.) */
__extension__ long long g = 1;
__extension__ enum E { A, B, };
__extension__ struct S { int n; int fam[]; };
int use(void) { return (int)g + A; }
