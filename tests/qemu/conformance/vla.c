







static int sum2d(int rows, int cols, int m[rows][cols])
{
    int s = 0;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            s += m[i][j];
    return s;
}

int main(void)
{
    int n = 5;
    int a[n];
    for (int i = 0; i < n; i++) a[i] = i * i;
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    if (s != 30) return 1;


    if (sizeof a != (unsigned)n * sizeof(int)) return 2;

    int rows = 3, cols = 4;
    int m[rows][cols];
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            m[i][j] = i * cols + j;
    if (sum2d(rows, cols, m) != 66) return 3;


    if (sizeof m[0] != (unsigned)cols * sizeof(int)) return 4;


    int k = n + cols;
    int b[k];
    for (int i = 0; i < k; i++) b[i] = i;
    if (b[k - 1] != 8) return 5;

    return 0;
}
