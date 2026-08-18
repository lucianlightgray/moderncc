/* T-mac-30191: `return;` with no value in a non-void function must be
 * diagnosed. mcc handled the prototyped forms (int f(void)/int f(int)) as an
 * error-by-default return-type diagnostic, but a K&R `int f()` (empty parens ->
 * func_old) took a special branch that silently synthesized `return 0` with NO
 * diagnostic even under -Wall -Werror. gcc/clang both error in all modes. This
 * file must be REJECTED. */
int f() { return; }
