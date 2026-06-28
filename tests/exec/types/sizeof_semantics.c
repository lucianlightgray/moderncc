



#include <stdio.h>

int counter = 0;
int bump(void) { counter++; return 0; }

int main(void)
{

    printf("char1: %d\n", (int)(sizeof(char) == 1));


    size_t z = sizeof(bump());
    printf("noeval: counter=%d nonzero_size=%d\n", counter, (int)(z > 0));


    printf("order: %d\n",
           (int)(sizeof(char)  <= sizeof(short) &&
                 sizeof(short) <= sizeof(int)   &&
                 sizeof(int)   <= sizeof(long)  &&
                 sizeof(long)  <= sizeof(long long)));


    printf("strlit: %d\n", (int)sizeof("hello"));


    int arr[10];
    printf("arr: %d elems=%d\n",
           (int)sizeof(arr), (int)(sizeof(arr) / sizeof(arr[0])));


    int k = 1;
    printf("expr: %d\n", (int)(sizeof(k + 1) == sizeof(int)));


    double d = 0;
    printf("match: %d\n", (int)(sizeof d == sizeof(double)));
    return 0;
}
