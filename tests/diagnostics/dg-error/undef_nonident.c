/* dg-error: macro name must be an identifier */
/* T-mac-30026.  #undef of a non-identifier (a number, punctuator, or an empty
 * operand) was silently accepted with no diagnostic, unlike #define which
 * rejects `< TOK_IDENT`.  gcc/clang error "macro names must be identifiers". */
#undef 123
int f(void) { return 0; }
