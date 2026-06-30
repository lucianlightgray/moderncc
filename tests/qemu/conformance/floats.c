


int main(void)
{
    double d = 0.1 + 0.2;
    if (d <= 0.29 || d >= 0.31) return 1;

    float f = 1.0f / 4.0f;
    if (f != 0.25f) return 2;

    double e = 2.0;
    if (e * e != 4.0) return 3;

    if ((int)3.9 != 3) return 4;
    if ((double)7 / 2 != 3.5) return 5;

    double neg = -1.5;
    if (neg + 1.5 != 0.0) return 6;

    float g = 16777216.0f;
    volatile float g1 = g + 1.0f;
    if (g1 != g) return 7;

    double big = 1e18;
    if ((long long)big != 1000000000000000000LL) return 8;

    return 0;
}
