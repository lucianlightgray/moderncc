/* T-mac-30192: _Alignof(void) is the same GNU void-size extension as
 * sizeof(void) — accepted at the default level, value 1 (was a hard error).
 * Exit 0 iff both are 1. */
int main(void){ return (sizeof(void) == 1 && _Alignof(void) == 1) ? 0 : 1; }
