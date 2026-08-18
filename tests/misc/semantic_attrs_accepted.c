/* T-mac-30181 / T-mac-30183: warn_unused_result, nonnull, returns_nonnull and
 * sentinel are standard function attributes gcc/clang accept; mcc emitted
 * "attribute ignored", so a file using them failed under -Werror. mcc does not
 * implement the associated diagnostics (that stays residual), but the
 * attributes must be ACCEPTED, not rejected. This must compile under -Wall
 * -Werror (also the __x__ spellings and argument forms). */
int wur(void) __attribute__((warn_unused_result));
void nn(void *p, void *q) __attribute__((nonnull(1, 2)));
void *rnn(void) __attribute__((returns_nonnull));
void sent(int first, ...) __attribute__((sentinel));
int wur2(void) __attribute__((__warn_unused_result__));
void nn2(void *p) __attribute__((__nonnull__(1)));
int wur(void) { return 1; }
void nn(void *p, void *q) { (void)p; (void)q; }
void *rnn(void) { static int x; return &x; }
void sent(int first, ...) { (void)first; }
int wur2(void) { return 2; }
void nn2(void *p) { (void)p; }
int main(void) { return 0; }
