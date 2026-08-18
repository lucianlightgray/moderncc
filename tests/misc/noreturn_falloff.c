/* T-mac-30184: a _Noreturn function that can fall off the end (control reaches
 * the closing brace without an unconditional noreturn call) must be diagnosed —
 * gcc "'noreturn' function does return", clang "function declared 'noreturn'
 * should not return". mcc warned only on an explicit `return`, not fall-off.
 * Here die() is called conditionally, so `bad` can return when x==0. */
_Noreturn void die(void);
_Noreturn void bad(int x) { if (x) die(); }
