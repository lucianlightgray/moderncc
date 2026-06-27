/* C99 array-declarator extensions in function parameters: type qualifiers and
   `static` inside the outermost array bound, plus VLA parameters whose size is
   passed alongside the array. */
#include <stdio.h>

/* `static 4` asserts at least 4 elements; `const` qualifies the (rewritten)
   pointer parameter. Both are syntactic C99 additions over C90. */
static int third(const int a[static 4])
{
    return a[3];
}

/* VLA parameter: the bound `n` names an earlier parameter */
static int row_sum(int n, int v[n])
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return s;
}

/* multidimensional VLA parameter: the inner dimension is a runtime value */
static int diag(int n, int m[n][n])
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += m[i][i];
    return s;
}

int main(void)
{
    int x[5] = { 1, 2, 3, 9, 5 };
    printf("third: %d\n", third(x));

    printf("row_sum: %d\n", row_sum(5, x));

    int grid[3][3] = { { 1, 0, 0 }, { 0, 2, 0 }, { 0, 0, 3 } };
    printf("diag: %d\n", diag(3, grid));
    return 0;
}
