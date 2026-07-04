#include <stdio.h>
__thread int tls_counter = 5;
int main(void) {
    tls_counter += 2;
    printf("%d\n", tls_counter);
    return 0;
}
