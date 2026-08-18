/* T-mac-30193 known-positive: same-signedness pointer assignments stay clean
 * under -Wall -Werror (and an explicit cast suppresses the mismatch). */
int main(void) {
	int i = 0;
	int *p = &i;
	unsigned int *u = (unsigned int *)&i;
	return (p != 0) + (u != 0) - 2;
}
