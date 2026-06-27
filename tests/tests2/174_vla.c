/* C99 6.7.5.2: variable length arrays. A block-scope array whose size is a
   non-constant expression is sized at runtime. sizeof on a VLA is evaluated at
   run time (6.5.3.4p2) and yields the actual byte size; VLAs work as function
   parameters and in multiple dimensions. */
#include <stdio.h>

/* VLA function parameter: n is in scope for the array's dimension */
static int row_sum(int n, int v[n])
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return s;
}

/* 2D VLA parameter */
static int grid_trace(int rows, int cols, int g[rows][cols])
{
    int t = 0;
    for (int i = 0; i < rows && i < cols; i++)
        t += g[i][i];
    return t;
}

int main(void)
{
    int n = 5;
    int a[n];                         /* VLA */
    for (int i = 0; i < n; i++)
        a[i] = (i + 1) * 2;           /* 2 4 6 8 10 */

    /* sizeof a VLA reflects its runtime length */
    printf("vsize: %d\n", (int)(sizeof a / sizeof a[0]));   /* 5 */
    printf("rowsum: %d\n", row_sum(n, a));                  /* 30 */

    /* 2D VLA */
    int r = 3, c = 4;
    int g[r][c];
    for (int i = 0; i < r; i++)
        for (int j = 0; j < c; j++)
            g[i][j] = i * 10 + j;
    printf("g22: %d\n", g[2][2]);     /* 22 */
    printf("trace: %d\n", grid_trace(r, c, g));   /* g00+g11+g22 = 0+11+22 = 33 */

    /* the size expression is evaluated where the declaration is reached */
    int k = 2;
    int b[k + 1];                     /* length 3 */
    printf("bsize: %d\n", (int)(sizeof b / sizeof b[0]));   /* 3 */
    return 0;
}
