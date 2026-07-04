#include <stdio.h>

enum Small { A,
             B,
             C };

int main(void) {
    printf("%ld\n", (long)__STDC_VERSION__);
    printf("%zu\n", sizeof(enum Small));
    return 0;
}
