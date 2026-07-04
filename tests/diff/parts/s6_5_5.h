int s6_5_5_side_count;
static int s6_5_5_side(int r) {
    s6_5_5_side_count++;
    return r;
}

void s6_5_5_binary_ops(void) {

    printf("prec %d %d %d %d %d\n",
           2 + 3 * 4, 1 << 2 + 1, (1 < 2) == 1, 5 & 3 | 4, 1 | 2 ^ 3 & 4);

    printf("assoc %d %d\n", 10 - 3 - 2, 100 / 5 / 2);

    printf("div %d %d %d %d\n", -7 / 2, 7 / -2, -7 % 2, 7 % -2);
    int ok = 1;
    for (int a = -9; a <= 9; a++)
        for (int b = -9; b <= 9; b++)
            if (b) {
                if ((a / b) * b + a % b != a)
                    ok = 0;
            }
    printf("ident %d\n", ok);

    printf("mul %d\n", 6 * 7);

    int arr[6] = {10, 20, 30, 40, 50, 60};
    int *p = arr;
    printf("padd %d %d\n", *(p + 3), *(3 + p));
    printf("psub %d\n", (int)(&arr[5] - &arr[1]));
    printf("pend %d\n", (arr + 6) == &arr[6]);
    ptrdiff_t dd = &arr[4] - arr;
    printf("pdiff %ld\n", (long)dd);

    printf("shl %u\n", 1u << 31);
    printf("shr %d %d\n", 240 >> 3, -16 >> 2);

    printf("rel %d %d %d %d\n", 3 < 5, 5 < 3, 5 <= 5, 7 >= 8);

    int *q = arr;
    printf("eq %d %d %d %d\n", 4 == 4, 4 != 4, (q == 0), (q == (void *)q));

    printf("bit %d %d %d\n", 0xF0 & 0x3C, 0xF0 ^ 0x3C, 0xF0 | 0x3C);

    printf("log %d %d %d %d\n", 3 && 4, 0 && 5, 0 || 0, 2 || 0);

    s6_5_5_side_count = 0;
    int r1 = (0 && s6_5_5_side(1));
    int r2 = (1 || s6_5_5_side(1));
    int r3 = (1 && s6_5_5_side(1));
    int r4 = (0 || s6_5_5_side(1));
    printf("sc %d %d %d %d cnt=%d\n", r1, r2, r3, r4, s6_5_5_side_count);
}
