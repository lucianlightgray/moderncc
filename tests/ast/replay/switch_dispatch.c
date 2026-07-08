/* AST replay: switch dispatch — value + cases + default + fall-through + break,
 * including a case range (docs/AST.md §5). Captured as If(op=6) with case/default
 * markers; replay rebuilds the switch_t and reproduces case_sort + gcase. */
static int classify(int x) {
	int r = 0;
	switch (x) {
	case 0:
		r += 1; /* fall-through into case 1 */
	case 1:
		r += 2;
		break;
	case 5 ... 7:
		r += 10;
		break;
	default:
		r = 100;
	}
	return r;
}

int main(void) {
	/* classify(0)=3, classify(1)=2, classify(6)=10, classify(1)=2 => 3+2+10+2 = 17 */
	return classify(0) + classify(1) + classify(6) + classify(1);
}
