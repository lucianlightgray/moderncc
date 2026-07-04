static int fib(int n) {
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static int square(int x) {
    return x * x;
}

int main(void) {
    if (fib(10) != 55)
        return 1;

    int s = 0, i;
    for (i = 1; i <= 100; i++)
        s += i;
    if (s != 5050)
        return 2;

    int (*fp)(int) = square;
    if (fp(9) != 81)
        return 3;

    int v = 2, r;
    switch (v) {
    case 1:
        r = 10;
        break;
    case 2:
        r = 20;
        break;
    default:
        r = 0;
        break;
    }
    if (r != 20)
        return 4;

    int k = 0;
    for (i = 0; i < 10; i++) {
        if (i == 5)
            goto done;
        k++;
    }
done:
    if (k != 5)
        return 5;

    return 0;
}
