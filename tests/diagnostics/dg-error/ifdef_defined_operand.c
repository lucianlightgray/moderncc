/* dg-error: invalid argument for '#ifdef' */
/* T-mac-30026.  #ifdef/#ifndef/#elifdef/#elifndef guarded only `< TOK_IDENT`,
 * so `defined` and `__VA_ARGS__` slipped through as operands and were silently
 * evaluated as "not defined" instead of being rejected like #define does. */
#ifdef defined
int a;
#endif
int f(void) { return 0; }
