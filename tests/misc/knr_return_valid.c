/* T-mac-30191 known-positive: every well-formed return must still compile, and
 * a K&R `int f()` with a proper value keeps returning it. Exit 0 on success. */
int knr() { return 5; }
void v() { return; }
int p(void) { return 7; }
int main(void) {
	v();
	if (knr() != 5) return 1;
	if (p() != 7) return 2;
	return 0;
}
