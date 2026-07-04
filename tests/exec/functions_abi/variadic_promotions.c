#include <stdio.h>
#include <stdarg.h>

static int sum_ints(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int s = 0;
    for (int i = 0; i < count; i++)
        s += va_arg(ap, int);
    va_end(ap);
    return s;
}

static double sum_doubles(int count, ...) {
    va_list ap;
    va_start(ap, count);
    double s = 0;
    for (int i = 0; i < count; i++)
        s += va_arg(ap, double);
    va_end(ap);
    return s;
}

int main(void) {
    char c = 5;
    short h = 100;

    printf("ints: %d\n", sum_ints(3, c, h, 1000));

    float f = 1.5f;
    double total = sum_doubles(3, f, 2.5, 4.0);
    printf("doubles: %d\n", (int)(total * 2));

    printf("mix: %d\n", sum_ints(4, 1, 2, 3, 4));
    return 0;
}
