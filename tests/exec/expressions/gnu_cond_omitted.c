/* GNU omitted-middle conditional `a ?: b`: the controlling expression is
   evaluated exactly once and reused as the result when truthy. */
#include <stdio.h>

static int side_effect_count;

static int eval(int v) { side_effect_count++; return v; }

int main(void)
{
    int v = eval(5) ?: eval(99);     /* truthy -> 5, eval() called once */
    printf("%d %d\n", v, side_effect_count);

    side_effect_count = 0;
    int z = eval(0) ?: eval(7);      /* falsy  -> 7, eval() called twice */
    printf("%d %d\n", z, side_effect_count);

    int n = 0;
    int w = (n++, 5) ?: 99;          /* comma side effect happens once */
    printf("%d %d\n", w, n);
    return 0;
}
