#include <stdio.h>
int asm_add(int);   /* defined in asmadd.s */
int main(void){ printf("%d\n", asm_add(41)); return 0; }
