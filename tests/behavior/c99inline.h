/* Plain C99 `inline` (no `extern`/`static`): the inline definition provides NO
   external out-of-line body on its own. Taking its address forces one. Under
   -fc99-inline-body mcc emits a WEAK out-of-line copy in every TU that needs it,
   so multiple TUs collapse onto a single definition at link (rather than a
   duplicate-symbol error, or an unresolved reference when no TU emits a body). */
inline int c99inline_add3(int a, int b, int c) { return a + b + c; }
