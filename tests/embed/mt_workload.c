/* Self-contained compilation/run workload for the multithreaded libmcc
   embedding stress test (api_threaded.c). It is compiled repeatedly and run
   concurrently across threads; the harness only checks the exit code, so the
   interleaved stdout is unimportant. Kept dependency-light so it compiles with
   just the bundled runtime headers. */

#include <stdio.h>

int mt_add(int a, int b) { return a + b; }
int mt_sub(int a, int b) { return a - b; }
int mt_mul(int a, int b) { return a * b; }

long mt_factorial(int n)
{
    long acc = 1;
    while (n > 1)
        acc *= n--;
    return acc;
}

int mt_gcd(int a, int b)
{
    while (b) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static int mt_table[64];

void mt_fill(int seed)
{
    int i;
    for (i = 0; i < 64; i++)
        mt_table[i] = mt_mul(i, seed) + mt_gcd(i, seed);
}

int main(int argc, char **argv)
{
    int seed = argc > 1 ? (int)argv[1][0] : 7;
    long sum = 0;
    int i;
    mt_fill(seed);
    for (i = 0; i < 64; i++)
        sum = mt_add(sum, mt_table[i]);
    printf("mt_workload: seed=%d sum=%ld fact=%ld\n", seed, sum, mt_factorial(8));
    return 0;
}
