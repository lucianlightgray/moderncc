



#include <stdio.h>

int main(void)
{

    printf("rel: %d %d %d %d\n", 3 < 5, 5 <= 5, 7 > 9, 9 >= 9);
    printf("eq: %d %d\n", 4 == 4, 4 != 4);


    printf("flt: %d %d\n", 1.5 < 2.0, 2.0 <= 1.5);



    printf("mixed: %d\n", (-1 > 0u));



    printf("prec: %d\n", 1 + 1 < 3);


    int a[4];
    int *p = &a[1], *q = &a[3];
    printf("ptr: %d %d %d\n", p < q, p == p, p != q);


    int *np = NULL;
    printf("null: %d %d\n", np == NULL, p != NULL);


    printf("paths: %d\n", (2 + 3) == (10 - 5));
    return 0;
}
