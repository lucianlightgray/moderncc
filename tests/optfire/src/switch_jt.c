extern int printf(const char *, ...);

/* A dense switch (8 contiguous cases + default) so switch_jt_dense() admits it
 * and gcase_jumptable() lowers it to an indexed jump table under
 * -fswitch-jumptable; -fno-switch-jumptable keeps the compare cascade. pick() is
 * a separate function (not inlined at -O2) so the switch survives to codegen. */
static int pick(int x)
{
	switch (x) {
	case 0: return 11;
	case 1: return 22;
	case 2: return 33;
	case 3: return 44;
	case 4: return 55;
	case 5: return 66;
	case 6: return 77;
	case 7: return 88;
	default: return -1;
	}
}

int main(void)
{
	long s = 0;
	int i;
	for (i = -2; i < 10; i++)
		s += pick(i);
	printf("switch_jt=%ld\n", s);
	return 0;
}
