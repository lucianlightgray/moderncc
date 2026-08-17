/* dg-error: 'i256' constant in preprocessor expression */
/* T-lin-10013.  #if arithmetic is intmax_t (C11 6.10.1p4); evaluating an i256
 * constant there would silently truncate to 64 bits, so it is refused. */
#if 340282366920938463463374607431768211456i256 > 0
int x;
#endif
int f(void) { return 0; }
