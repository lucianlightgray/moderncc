/* AST replay: named goto/labels — forward and backward jumps — replay via label
 * markers (Jump op==4 label / op==5 goto) and a replay-time label table that
 * reproduces the parser's forward-chain / backward-jump / definition-backpatch
 * (docs/AST.md §5). */
int main(void) {
	int i = 0, s = 0;
	if (i < 0)
		goto neg; /* forward, never taken */
loop:
	if (i >= 9)
		goto done; /* forward */
	s += i;
	i++;
	goto loop; /* backward */
neg:
	return -1;
done:
	return s + 6; /* 0+1+..+8 = 36, +6 = 42 */
}
