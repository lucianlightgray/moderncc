/* Regression for T-mac-30099: an exhaustive switch (every arm returns +
 * default, no break) lowered via the jump-table strategy must NOT leave the
 * function end wrongly reachable — pre-fix mcc emitted a spurious hard error
 * "function might return no value" under MCC_SWITCH_JUMPTABLE=1 / -O13. Run
 * this under the jump-table env; it must compile, run, and return 0. The value
 * checks also guard the jump-table dispatch itself against a miscompile. */
#include <stdio.h>

static int dense(int x) {
	switch (x) {
	case 0: return 10;
	case 1: return 11;
	case 2: return 12;
	case 3: return 13;
	case 4: return 14;
	default: return -1;
	}
}

/* non-contiguous / negative cases exercise a wider jump-table span */
static int sparse(int x) {
	switch (x) {
	case -2: return 100;
	case 0: return 200;
	case 5: return 300;
	case 6: return 301;
	case 7: return 302;
	default: return 999;
	}
}

int main(void) {
	int fails = 0, checks = 0;
	int de[] = {10, 11, 12, 13, 14};
	for (int i = 0; i < 5; i++) { checks++; if (dense(i) != de[i]) fails++; }
	checks++; if (dense(9) != -1) fails++;
	checks++; if (dense(-1) != -1) fails++;
	checks++; if (sparse(-2) != 100) fails++;
	checks++; if (sparse(0) != 200) fails++;
	checks++; if (sparse(6) != 301) fails++;
	checks++; if (sparse(42) != 999) fails++;
	printf("jt-exhaustive checks=%d fails=%d\n", checks, fails);
	return fails;
}
