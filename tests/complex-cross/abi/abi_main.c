


#include <stdio.h>
#ifndef ABI_BASE
#define ABI_BASE double
#endif
typedef ABI_BASE _Complex C;

extern C tcc_op(C, C);
extern C tcc_calls_gcc(C, C);
extern C tcc_mix(int, C, ABI_BASE);

C gcc_op(C a, C b){ return a + b; }

static C mk(ABI_BASE re, ABI_BASE im){ C z; __real__ z = re; __imag__ z = im; return z; }
static int eq(C x, ABI_BASE re, ABI_BASE im){ return __real__ x == re && __imag__ x == im; }

static int fail = 0;
#define CHK(cond, name) do{ if(!(cond)){ printf("MISMATCH: %s\n", name); fail = 1; } }while(0)

int main(void)
{
    C a = mk(1, 2), b = mk(3, 4);
    CHK(eq(tcc_op(a, b),        4, 6), "tcc_op");
    CHK(eq(tcc_calls_gcc(a, b), 4, 6), "tcc_calls_gcc");
    CHK(eq(tcc_mix(3, a, 4),    8, 2), "tcc_mix");
    if (!fail) printf("ok\n");
    return fail;
}
