



#include <stdio.h>


static int row_sum(int n, int v[n])
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += v[i];
    return s;
}


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
    int a[n];
    for (int i = 0; i < n; i++)
        a[i] = (i + 1) * 2;


    printf("vsize: %d\n", (int)(sizeof a / sizeof a[0]));
    printf("rowsum: %d\n", row_sum(n, a));


    int r = 3, c = 4;
    int g[r][c];
    for (int i = 0; i < r; i++)
        for (int j = 0; j < c; j++)
            g[i][j] = i * 10 + j;
    printf("g22: %d\n", g[2][2]);
    printf("trace: %d\n", grid_trace(r, c, g));


    int k = 2;
    int b[k + 1];
    printf("bsize: %d\n", (int)(sizeof b / sizeof b[0]));
    return 0;
}
