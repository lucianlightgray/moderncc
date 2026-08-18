/* T-mac-30133: __extension__ silences -pedantic for the ENTIRE declaration it
 * prefixes — declspec (long long), enum/struct body (trailing comma, flexible
 * array), _Static_assert, and the declarator (VLA) — without leaking to later
 * declarations. */
__extension__ long long g = 1;
__extension__ enum E { A, B, };
__extension__ struct S { int n; int fam[]; };
__extension__ _Static_assert(1, "ok");
int f(int n) { __extension__ int v[n]; v[0] = 1; return v[0]; }
int use(void) { return (int)g + A + f(2); }
