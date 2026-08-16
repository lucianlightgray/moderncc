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

/* Non-constant __except filter: reads a global, so it cannot be a raw DWORD in the
   SCOPE_TABLE and must compile to a filter funclet the dispatcher calls. */
volatile int g_want = 1;
static int guard_nonconst(volatile int *p) {
	int caught = 0;
	__try {
		*p = 42;      /* faults when p is null */
	} __except(g_want) {  /* g_want==1 -> EXCEPTION_EXECUTE_HANDLER via the funclet */
		caught = 55;
	}
	return caught;
}

/* Filter reads a PARENT LOCAL (a parameter): the funclet must reach the faulting
   frame through the establisher frame (rbp = rdx) so [rbp+off] resolves `sel`. */
static int guard_local(volatile int *p, int sel) {
	int caught = 0;
	__try {
		*p = 42;
	} __except(sel) {  /* sel!=0 -> execute handler; sel==0 -> continue search */
		caught = 88;
	}
	return caught;
}

/* Nested: the inner non-constant filter evaluates to 0 (EXCEPTION_CONTINUE_SEARCH),
   so the fault must propagate out of the inner funclet to the outer handler. */
static int guard_nested(volatile int *p) {
	int caught = 0;
	__try {
		__try {
			*p = 42;
		} __except(g_want - 1) {  /* 0 -> continue search, inner handler skipped */
			caught = 1;
		}
	} __except(1) {
		caught = 77;
	}
	return caught;
}

/* Parent-local filter that returns 0 (continue search) from a nested inner __try,
   so the outer handler catches — exercises establisher-frame local read + propagation. */
static int guard_local_ns(volatile int *p, int sel) {
	int caught = 0;
	__try {
		__try {
			*p = 42;
		} __except(sel) {  /* sel==0 -> continue search */
			caught = 1;
		}
	} __except(1) {
		caught = 99;
	}
	return caught;
}

int main(void) {
	int f_av, f_ok, f_div, f_divok, f_nc, f_ns, f_lo, f_lns;
	int x = 11;

	f_av = guard_av(0);          /* null store -> caught -> 7 */
	f_ok = guard_av(&x);         /* no fault -> 100, and x becomes 42 */
	f_div = guard_div(10, 0);    /* div by zero -> caught -> 9 */
	f_divok = guard_div(20, 4);  /* no fault -> 5 */
	f_nc = guard_nonconst(0);    /* non-const filter funclet catches -> 55 */
	f_ns = guard_nested(0);      /* inner funclet continues search, outer catches -> 77 */
	f_lo = guard_local(0, 1);    /* parent-local filter (sel=1) catches -> 88 */
	f_lns = guard_local_ns(0, 0);/* parent-local filter (sel=0) continues, outer -> 99 */

	printf("av=%d ok=%d x=%d div=%d divok=%d nc=%d ns=%d lo=%d lns=%d\n",
			f_av, f_ok, x, f_div, f_divok, f_nc, f_ns, f_lo, f_lns);
	return (f_av == 7 && f_ok == 100 && x == 42 && f_div == 9 && f_divok == 5 &&
			f_nc == 55 && f_ns == 77 && f_lo == 88 && f_lns == 99) ? 0 : 1;
}
