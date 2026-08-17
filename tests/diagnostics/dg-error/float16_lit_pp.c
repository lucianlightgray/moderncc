/* dg-error: invalid constant in preprocessor expression */
/* T-mac-30022.  #if arithmetic is an integer constant expression (C11
 * 6.10.1p4); a floating constant is not permitted.  mcc rejects the plain
 * float suffixes (1.0/1.0f/1.0L via TOK_CFLOAT..TOK_CLDOUBLE) but the
 * _Float16 suffix lexes to TOK_CFLOAT16, which sat outside that range and
 * slipped an illegal float operand into the integer evaluator.  Refuse it. */
#if 1.0f16 > 0
int x;
#endif
int f(void) { return 0; }
