/* T-mac-30125: __builtin_assume must parse+typecheck its operand (not a no-op
 * macro) — an undeclared identifier is a hard error like clang. */
int main(void) { __builtin_assume(undeclared_xyz_30125 > 0); return 0; }
