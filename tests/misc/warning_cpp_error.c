/* T-mac-30220: -Werror=cpp must promote #warning to an error (mcc treated
 * -Werror=cpp as unsupported and did not escalate). This must be REJECTED. */
#warning this should become an error under -Werror=cpp
int main(void) { return 0; }
