/* T-mac-30220: `#warning` must be under the -Wcpp category so -Wno-cpp
 * suppresses it (gcc/clang do; mcc emitted it unconditionally and treated
 * -Wno-cpp as an unsupported option). Compiled with -Wno-cpp -Werror this must
 * succeed -- the #warning is suppressed, so -Werror has nothing to promote. */
#warning this should be suppressed by -Wno-cpp
int main(void) { return 0; }
