/* T-mac-30161: `#pragma GCC warning "msg"` must emit the message as a warning;
 * mcc silently dropped it. Compiles (warning only). */
#pragma GCC warning "diagnostic message here"
int main(void) { return 0; }
