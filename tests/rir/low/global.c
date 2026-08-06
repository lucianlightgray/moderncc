/* A Ref carrying VT_SYM is a host link-time address, not a value. */
extern int g;

int f(int a) { return g + a; }
