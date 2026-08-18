/* T-mac-30103: shadowed locals must land in nested lexical blocks (a debugger's
 * innermost-scope lookup resolves the right instance), not one level too shallow.
 * The two `v` here must be in distinct, nested DW_TAG_lexical_block DIEs. */
int h(int a) { int v = a; { int v = a + 100; a = v; } return v + a; }
int main(void) { return h(1); }
