/* The forward-inline re-emit, minimally.
 *
 * `outer` calls a static `inner` that is DEFINED BELOW IT, so at the point
 * `outer`'s body is finished `inner` is not yet graftable.  ast_reemit_retain
 * records `outer`; ast_reemit_forward_inlines re-emits it at end of TU once
 * `inner` exists, appending the new body at the end of .text and leaving the
 * original where it was.  Reversing the two definitions removes the orphan and
 * is the control the write-up quotes.
 *
 * Three chained pairs, so the cell fails on more than one function if the FDE
 * is dropped, and so ast.orphan_fn stays above its floor if one shape stops
 * qualifying. */

static int inner_a(int x);
static int inner_b(int x);
static int inner_c(int x);

int outer_a(int x) { return x <= 0 ? 0 : inner_a(x - 1); }
static int inner_a(int x) { return x + outer_a(x - 1); }

int outer_b(int x) { return x <= 0 ? 1 : inner_b(x - 1) + 2; }
static int inner_b(int x) { return x * 3 + outer_b(x - 1); }

int outer_c(int x, int y) { return y ? inner_c(x) : x; }
static int inner_c(int x) { return x ^ outer_c(x - 1, 0); }

int main(void) { return outer_a(3) + outer_b(2) + outer_c(1, 1); }
