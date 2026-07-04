#include <stdio.h>

int main(void) {

    printf("trunc: %d %d\n", (int)3.99, (int)-3.99);
    printf("widen: %d\n", (int)((double)7 == 7.0));

    int big = 0x4321;
    printf("narrow: %u\n", (unsigned)(unsigned char)big);

    int q = 5;
    (void)q;
    (void)(q + 1);
    printf("voidcast: %d\n", q);

    printf("intdiv: %d\n", 7 / 2);
    printf("fltdiv: %d\n", (int)(((double)7 / 2) * 10));

    double avg = (double)(3 + 4) / 2;
    printf("avg: %d\n", (int)(avg * 2));

    int obj = 1234;
    int *p = &obj;
    unsigned long bits = (unsigned long)(void *)p;
    int *p2 = (int *)(void *)bits;
    (void)p2;

    unsigned long long full = (unsigned long long)(void *)p;
    printf("ptrrt: %d\n", *(int *)(void *)full);
    return 0;
}
