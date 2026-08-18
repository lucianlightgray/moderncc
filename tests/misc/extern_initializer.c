/* T-mac-30187: `extern int x = 5;` at file scope is a definition, and combining
 * `extern` with an initializer is diagnosed by gcc/clang ("'x' initialized and
 * declared 'extern'"); mcc was silent. The value is still emitted correctly. */
extern int x = 5;
int main(void) { return x - 5; }
