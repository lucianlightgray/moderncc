/* macro_nesting.c — CST slice-J macro-invocation v1 boundary fixture.
 *
 * Two documented v1 imprecisions in how a function-like invocation maps to a
 * CST_MacroInvocation node. The written source still round-trips byte-identically
 * (the load-bearing invariant); only the node shape is approximate:
 *
 *   1. The trailing `)` of a function-like invocation is NOT inside the
 *      MacroInvocation span — it splits off into a sibling Paren node. So
 *      `OUTER(INNER)` wraps `OUTER(INNER` and a separate Paren holds `)`.
 *   2. An object-like macro used inside another macro's argument list stays a
 *      plain token — `INNER` here does not become its own MacroInvocation node.
 *
 * macro-nesting.cmake pins both as the accepted v1 boundary: it asserts the
 * round-trip is clean, that there is exactly ONE MacroInvocation (INNER unwrapped
 * = imprecision 2), and that a Paren node follows the invocation's tokens
 * (trailing `)` split = imprecision 1). A future precise slice-J expander would
 * change the node shape and flip these assertions. */

#define INNER 5
#define OUTER(x) ((x) + 1)

int g(void) {
	return OUTER(INNER);
}
