/* T-mac-30184 known-positive: a _Noreturn function whose body ends in an
 * unconditional noreturn call does NOT return and must stay clean, even under
 * -Werror; a normal void function is unaffected. */
_Noreturn void die(void);
_Noreturn void good(void) { die(); }
void plain(void) { }
int main(void) { plain(); return 0; }
