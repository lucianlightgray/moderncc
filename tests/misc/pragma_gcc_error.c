/* T-mac-30161: `#pragma GCC error "msg"` must stop the compile with the message;
 * mcc silently dropped it. This file must be REJECTED. */
#pragma GCC error "intentional compile stop"
int main(void) { return 0; }
