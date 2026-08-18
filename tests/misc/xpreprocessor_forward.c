/* T-mac-30228: -Xpreprocessor ARG must forward ARG to the preprocessor (like
 * -Wp,ARG); build systems pass it, and it hard-errored on mcc. Run as
 *   mcc -Xpreprocessor -DFOO=41 -run this.c
 * -> exits 0 only when FOO was defined (and reaches 42) via the forwarded -D. */
#ifndef FOO
#error "FOO not forwarded by -Xpreprocessor"
#endif
int main(void) { return (FOO + 1 == 42) ? 0 : 1; }
