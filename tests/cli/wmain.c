#include <stdio.h>
int wsym(void) __attribute__((weak));
int wsym(void){ return 99; }       /* weak: overridden by wstrong.c's strong def */
int main(void){ printf("%d\n", wsym()); return 0; }
