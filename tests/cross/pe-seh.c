#include <stdio.h>

static int guard_av(volatile int *p) {
	int caught = 0;
	__try {
		*p = 42;      /* faults when p is null */
		caught = 100; /* not reached on fault */
	} __except(1) {
		caught = 7;
	}
	return caught;
}

static int guard_div(int a, int b) {
	int r = -5;
	__try {
		r = a / b;    /* faults when b is 0 */
	} __except(1) {
		r = 9;
	}
	return r;
}

int main(void) {
	int f_av, f_ok, f_div, f_divok;
	int x = 11;

	f_av = guard_av(0);         /* null store -> caught -> 7 */
	f_ok = guard_av(&x);        /* no fault -> 100, and x becomes 42 */
	f_div = guard_div(10, 0);   /* div by zero -> caught -> 9 */
	f_divok = guard_div(20, 4); /* no fault -> 5 */

	printf("av=%d ok=%d x=%d div=%d divok=%d\n", f_av, f_ok, x, f_div, f_divok);
	return (f_av == 7 && f_ok == 100 && x == 42 && f_div == 9 && f_divok == 5) ? 0 : 1;
}
