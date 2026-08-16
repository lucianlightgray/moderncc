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

/* __finally runs on the NORMAL exit of the __try; the body reads a parent local
   (`ran`) through the establisher frame. */
static int guard_finally(int *ran) {
	int r = 3;
	__try {
		r = 5;
	} __finally {
		*ran = 1;
	}
	return r;
}

/* __finally runs during UNWIND: the inner __try faults, the outer __except catches,
   and the inner __finally must have run on the way out. */
static int guard_finally_unwind(volatile int *p, int *ran) {
	int caught = 0;
	__try {
		__try {
			*p = 42;   /* faults */
		} __finally {
			*ran = 1;  /* must run as the exception unwinds through it */
		}
	} __except(1) {
		caught = 66;
	}
	return caught;
}

/* slice 3b: EARLY EXITS out of a __try must run the __finally. The return
   value is computed BEFORE the finally runs (cl semantics), so the finally's
   write to `v` must not change what is returned. */
static int fin_ret(int *ran) {
	int v = 31;
	__try {
		return v + 1;   /* 32 is saved, then the finally runs */
	} __finally {
		v = 1000;       /* must not affect the already-computed return value */
		*ran += 1;
	}
	return v;           /* not reached */
}

/* break out of a __try/__finally inside a loop: the finally runs on the break
   iteration too (3 fallthrough exits + 1 break exit = 4 runs). */
static int fin_break(int *ran) {
	int i, r = 0;
	for (i = 0; i < 10; i++) {
		__try {
			if (i == 3) break;
			r += 1;
		} __finally {
			*ran += 1;
		}
	}
	return r * 100 + i;   /* r=3, i=3 -> 303 */
}

/* continue out of a __try/__finally: the finally runs on every iteration,
   whether the exit is the continue or the fallthrough. */
static int fin_cont(int *ran) {
	int i, r = 0;
	for (i = 0; i < 5; i++) {
		__try {
			if (i & 1) continue;
			r += 10;
		} __finally {
			*ran += 1;
		}
	}
	return r;   /* i=0,2,4 add -> 30; finally ran 5x */
}

/* goto out of a __try/__finally: the finally runs, then control reaches the
   label — the skipped assignment between the __try and the label must not run. */
static int fin_goto(int *ran) {
	int r = 2;
	__try {
		goto out;
	} __finally {
		*ran += 1;
	}
	r = 50;   /* jumped over */
out:
	return r;   /* 2 */
}

/* return through TWO nested __try/__finally: both finallys run, inner first. */
static int fin_nest(int *ran) {
	__try {
		__try {
			return 8;
		} __finally {
			*ran = *ran * 10 + 1;   /* inner runs first: 0 -> 1 */
		}
	} __finally {
		*ran = *ran * 10 + 2;       /* then the outer: 1 -> 12 */
	}
	return 0;   /* not reached */
}

int main(void) {
	int f_av, f_ok, f_div, f_divok, f_nc, f_ns, f_lo, f_lns;
	int f_fin, f_finr = 0, f_fu, f_fur = 0;
	int f_ret, f_retr = 0, f_brk, f_brkr = 0, f_cont, f_contr = 0;
	int f_gt, f_gtr = 0, f_nst, f_nstr = 0;
	int x = 11;

	f_av = guard_av(0);          /* null store -> caught -> 7 */
	f_ok = guard_av(&x);         /* no fault -> 100, and x becomes 42 */
	f_div = guard_div(10, 0);    /* div by zero -> caught -> 9 */
	f_divok = guard_div(20, 4);  /* no fault -> 5 */
	f_nc = guard_nonconst(0);    /* non-const filter funclet catches -> 55 */
	f_ns = guard_nested(0);      /* inner funclet continues search, outer catches -> 77 */
	f_lo = guard_local(0, 1);    /* parent-local filter (sel=1) catches -> 88 */
	f_lns = guard_local_ns(0, 0);/* parent-local filter (sel=0) continues, outer -> 99 */
	f_fin = guard_finally(&f_finr);       /* normal exit runs __finally -> r=5, finr=1 */
	f_fu = guard_finally_unwind(0, &f_fur);/* unwind runs inner __finally -> caught=66, fur=1 */
	f_ret = fin_ret(&f_retr);             /* early return -> 32, finally ran once */
	f_brk = fin_break(&f_brkr);           /* break exit -> 303, finally ran 4x */
	f_cont = fin_cont(&f_contr);          /* continue exit -> 30, finally ran 5x */
	f_gt = fin_goto(&f_gtr);              /* goto exit -> 2, finally ran once */
	f_nst = fin_nest(&f_nstr);            /* nested return -> 8, inner-then-outer = 12 */

	printf("av=%d ok=%d x=%d div=%d divok=%d nc=%d ns=%d lo=%d lns=%d fin=%d finr=%d fu=%d fur=%d "
			"ret=%d retr=%d brk=%d brkr=%d cont=%d contr=%d gt=%d gtr=%d nst=%d nstr=%d\n",
			f_av, f_ok, x, f_div, f_divok, f_nc, f_ns, f_lo, f_lns, f_fin, f_finr, f_fu, f_fur,
			f_ret, f_retr, f_brk, f_brkr, f_cont, f_contr, f_gt, f_gtr, f_nst, f_nstr);
	return (f_av == 7 && f_ok == 100 && x == 42 && f_div == 9 && f_divok == 5 &&
			f_nc == 55 && f_ns == 77 && f_lo == 88 && f_lns == 99 &&
			f_fin == 5 && f_finr == 1 && f_fu == 66 && f_fur == 1 &&
			f_ret == 32 && f_retr == 1 && f_brk == 303 && f_brkr == 4 &&
			f_cont == 30 && f_contr == 5 && f_gt == 2 && f_gtr == 1 &&
			f_nst == 8 && f_nstr == 12) ? 0 : 1;
}
