/* GAS data directives via file-scope asm, read back from C:
   .rept/.endr, .fill, .skip, .asciz vs .ascii, .set, and a symbol-relative
   .long (local-label difference 2b-1b). Requires the integrated assembler. */
#include <stdio.h>

extern unsigned char  rep[4];
extern unsigned short fillv[4];
extern unsigned char  skipd[5];
extern char           asciz_s[];
extern char           ascii_s[];
extern unsigned int   delta;
extern int            aliased;

asm(
    ".data\n"
    ".globl rep\n"
    "rep: .rept 4\n .byte 0x7\n .endr\n"
    ".globl fillv\n"
    "fillv: .fill 4,2,0x1234\n"
    ".globl skipd\n"
    "skipd: .byte 0xAA\n .skip 3,0xBB\n .byte 0xCC\n"
    ".globl asciz_s\n"
    "asciz_s: .asciz \"ab\"\n"        /* keeps trailing NUL -> {'a','b',0} */
    ".globl ascii_s\n"
    "ascii_s: .ascii \"cd\"\n .byte 0\n"  /* .ascii drops NUL; we add one */
    ".globl delta\n"
    "1: .long 0\n"
    "2: .long 0\n"
    "delta: .long 2b-1b\n"            /* local-label difference == 4 */
    ".globl realsym\n"
    "realsym: .long 99\n"
    ".set aliased, realsym\n"         /* .set symbol aliasing */
);

int main(void)
{
    printf("rep: %d %d %d %d\n", rep[0], rep[1], rep[2], rep[3]);
    printf("fill: %d %d\n", fillv[0], fillv[3]);
    printf("skip: %d %d %d %d %d\n",
           skipd[0], skipd[1], skipd[2], skipd[3], skipd[4]);
    printf("str: %s|%s\n", asciz_s, ascii_s);
    printf("delta: %u\n", delta);
    printf("alias: %d\n", aliased);
    return 0;
}
