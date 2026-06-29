/* GNU lvalue-cast extension in an asm operand: `"=r" ((unsigned)(r))` must stay
   valid even though §6.5.4 makes `(int)a = 9` a constraint violation. mcc's own
   runtime/lib/libmcc1.c relies on this (udiv_qrnnd on i386), so it must keep
   compiling; gcc/clang accept lvalue casts in asm operands too. */
extern int printf(const char *, ...);

int main(void)
{
    unsigned x = 5, r = 0;
    __asm__ ("movl %1, %0" : "=r" ((unsigned)(r)) : "r" ((unsigned)(x)));
    printf(r == 5 ? "OK\n" : "FAIL\n");
    return 0;
}
