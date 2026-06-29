/* Self-checking variable-length-array conformance (C99 6.7.6.2). 1-D and 2-D
   variably-modified arrays, runtime sizeof on a VLA, and a VLA function
   parameter all need frame-relative addressing plus runtime size/stride
   arithmetic that fixed arrays never exercise -- and the 2-D row stride is a
   multiply the codegen has to thread through each subscript. Endianness-
   independent; 0 on success. */

/* VLA parameter: m's row stride (cols) is a runtime value passed alongside it */
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
    int a[n];                               /* 1-D VLA */
    for (int i = 0; i < n; i++) a[i] = i * i;
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    if (s != 30) return 1;                  /* 0+1+4+9+16 */

    /* sizeof on a VLA is evaluated at runtime, not a constant */
    if (sizeof a != (unsigned)n * sizeof(int)) return 2;

    int rows = 3, cols = 4;
    int m[rows][cols];                      /* 2-D VLA */
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            m[i][j] = i * cols + j;
    if (sum2d(rows, cols, m) != 66) return 3;   /* sum 0..11 */

    /* row sizeof picks up the inner runtime dimension */
    if (sizeof m[0] != (unsigned)cols * sizeof(int)) return 4;

    /* a VLA sized by a value computed at runtime, then indexed near its end */
    int k = n + cols;                       /* 9 */
    int b[k];
    for (int i = 0; i < k; i++) b[i] = i;
    if (b[k - 1] != 8) return 5;

    return 0;
}
