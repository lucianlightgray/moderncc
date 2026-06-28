#include <stdio.h>
int target(void){ return 34; }
int my_alias(void) __attribute__((alias("target")));
int main(void){ printf("%d %d\n", my_alias(), my_alias() == target()); return 0; }
