#include <stdio.h>

int main(void) {
    float inf = __inf__;
    float nan = __nan__;
    printf("%d %d %d\n",
           inf > 1e30f,
           inf == inf,
           nan != nan);
    return 0;
}
