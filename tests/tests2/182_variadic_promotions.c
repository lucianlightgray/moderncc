/* C99 6.5.2.2p6-7: in a call to a function with a trailing `...`, the arguments
   in the variable part undergo the default argument promotions -- integer types
   narrower than int promote to int (or unsigned int), and float promotes to
   double. So a variadic callee must read a promoted char/short as int and a
   promoted float as double via <stdarg.h>. */
#include <stdio.h>
#include <stdarg.h>

/* sum `count` arguments read as int (chars/shorts arrive promoted to int) */
static int sum_ints(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int s = 0;
    for (int i = 0; i < count; i++)
        s += va_arg(ap, int);
    va_end(ap);
    return s;
}

/* read floating arguments as double (a float argument is promoted to double) */
static double sum_doubles(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    double s = 0;
    for (int i = 0; i < count; i++)
        s += va_arg(ap, double);
    va_end(ap);
    return s;
}

int main(void)
{
    char  c = 5;
    short h = 100;

    /* char and short promote to int in the variadic call; read them as int */
    printf("ints: %d\n", sum_ints(3, c, h, 1000));   /* 1105 */

    /* a float argument is promoted to double; read it as double */
    float f = 1.5f;
    double total = sum_doubles(3, f, 2.5, 4.0);      /* 8.0 */
    printf("doubles: %d\n", (int)(total * 2));       /* 16 */

    /* mixing literal ints (already int) confirms the int path */
    printf("mix: %d\n", sum_ints(4, 1, 2, 3, 4));    /* 10 */
    return 0;
}
