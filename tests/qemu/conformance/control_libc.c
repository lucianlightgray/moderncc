/* Self-checking control-flow ABI conformance against the target libc:
   setjmp/longjmp and a qsort() callback. setjmp/longjmp must save and restore
   the platform's callee-saved register set exactly the way the target libc's
   setjmp expects -- a from-scratch compiler that picks the wrong callee-saved
   set, or spills a value the longjmp clobbers, fails here even though plain
   recursion (control.c) passes. qsort drives a function pointer *back into our
   code from inside libc*, exercising the indirect-call ABI across the libc
   boundary. Endianness-independent; 0 on success. */

#include <setjmp.h>
#include <stdlib.h>

static jmp_buf jb;

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* Recurse, holding live values in registers across the call, then longjmp out
   of the deepest frame -- the saved context in main() must be restored intact. */
static void deep(int n)
{
    volatile int local = n * 2;
    if (n == 0)
        longjmp(jb, 42);
    deep(n - 1);
    (void)local;
}

int main(void)
{
    /* values that must survive the longjmp unwind: one in memory (volatile),
       one the compiler is free to keep in a callee-saved register */
    volatile int marker = 12345;
    int keep = 678;

    int rc = setjmp(jb);
    if (rc == 0) {
        deep(5);
        return 1;                 /* unreachable: deep() always longjmps */
    }
    if (rc != 42) return 2;       /* longjmp value propagated */
    if (marker != 12345) return 3;
    if (keep != 678) return 4;    /* callee-saved value survived */

    /* qsort callback: libc calls cmp_int through a function pointer */
    int arr[] = { 5, 3, 8, 1, 9, 2, 7, 4, 6, 0 };
    qsort(arr, sizeof arr / sizeof arr[0], sizeof arr[0], cmp_int);
    for (int i = 0; i < 10; i++)
        if (arr[i] != i) return 5;

    return 0;
}
