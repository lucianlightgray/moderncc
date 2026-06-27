/* C99 compound literals: (type){ init } -- as initializers, function
   arguments, modifiable lvalues, array literals, nested, and block-scoped
   lifetime in a loop. */
#include <stdio.h>

struct point { int x, y; };

static int sum_point(struct point p) { return p.x + p.y; }
static int dot(const int *a, const int *b, int n)
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += a[i] * b[i];
    return s;
}

int main(void)
{
    /* array compound literal bound to a pointer */
    int *a = (int[]){ 10, 20, 30, 40 };
    printf("array: %d %d %d %d\n", a[0], a[1], a[2], a[3]);

    /* struct compound literal, address taken, modified through pointer */
    struct point *p = &(struct point){ .x = 3, .y = 4 };
    p->x += 10;
    printf("struct: %d %d\n", p->x, p->y);

    /* compound literal passed directly as an argument */
    printf("arg sum: %d\n", sum_point((struct point){ 5, 6 }));

    /* array compound literals as arguments */
    printf("dot: %d\n", dot((int[]){ 1, 2, 3 }, (int[]){ 4, 5, 6 }, 3));

    /* nested compound literals */
    struct rect { struct point lo, hi; } r =
        (struct rect){ .lo = (struct point){ 1, 1 }, .hi = (struct point){ 4, 5 } };
    printf("rect area: %d\n", (r.hi.x - r.lo.x) * (r.hi.y - r.lo.y));

    /* each loop iteration gets a fresh, independent literal object */
    int total = 0;
    for (int i = 0; i < 4; i++) {
        int *cell = (int[]){ 0, 0 };
        cell[0] = i;
        cell[1] = i * i;
        total += cell[0] + cell[1];
    }
    printf("loop total: %d\n", total);

    /* sizeof a compound literal is the array's size, not a pointer's */
    printf("sizeof literal: %d\n", (int)sizeof((int[]){ 1, 2, 3, 4, 5 }));
    return 0;
}
