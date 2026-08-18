/* T-mac-30130 slice-1 known-positive: labels reached by goto or whose address
 * is taken (&&label) are USED and must stay clean under -Wall -Werror. */
int main(void) {
	int i = 0;
	void *p = &&addr_lbl;
	(void)p;
loop:
	i++;
	if (i < 3) goto loop;
addr_lbl:
	return (i == 3) ? 0 : 1;
}
