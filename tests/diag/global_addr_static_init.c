/* Positive companion for T-mac-30118: the address of an ordinary global IS a
 * compile-time constant and must still be accepted as a static initializer. */
int g = 5;
int *p = &g;
