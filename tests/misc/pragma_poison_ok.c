/* T-mac-30160 known-positive: identifiers NOT poisoned must still compile, and
 * poisoning one name must not affect others. Exit 0. */
#pragma GCC poison never_used_name
int allowed = 5;
int main(void) { int also_fine = 7; return (allowed == 5 && also_fine == 7) ? 0 : 1; }
