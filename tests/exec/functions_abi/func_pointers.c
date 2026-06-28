




#include <stdio.h>

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }


static int apply(int (*f)(int, int), int x, int y) { return f(x, y); }

int main(void)
{
    int (*fp)(int, int) = add;


    printf("calls: %d %d %d\n", fp(2, 3), (*fp)(2, 3), (&add)(2, 3));


    printf("same: %d\n", (add == &add));


    printf("apply: %d %d\n", apply(add, 10, 4), apply(sub, 10, 4));


    int (*table[2])(int, int) = { add, sub };
    int total = 0;
    for (int i = 0; i < 2; i++)
        total += table[i](20, 5);
    printf("table: %d\n", total);


    fp = sub;
    printf("rebind: %d\n", fp(9, 4));
    return 0;
}
