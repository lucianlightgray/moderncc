


#include <stdio.h>


#define A   B
#define B   C
#define C   17



int selfref = 5;
#define selfref (selfref + 100)


#define DOUBLE(x)  ((x) + (x))
#define APPLY(f, v) f(v)



#define INC(x) ((x) + 1)

int main(void)
{
    printf("chain: %d\n", A);
    printf("selfref: %d\n", selfref);
    printf("rescan: %d\n", APPLY(DOUBLE, 9));
    printf("space_paren: %d\n", INC (41));



    printf("nested: %d\n", DOUBLE(DOUBLE(3)));
    return 0;
}
