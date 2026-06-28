


#include <stdio.h>



static int third(const int a[static 4])
{
    return a[3];
}


static int row_sum(int n, int v[n])
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return s;
}


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
