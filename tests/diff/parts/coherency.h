enum coh_e { COH_A,
             COH_B,
             COH_C = 10,
             COH_D };
struct coh_s {
    unsigned tag : 4;
    signed val : 12;
};
static int coh_generic_tag(int x) {
    return 1;
}
static int coh_generic_tag_d(double x) {
    return 2;
}
#define COH_TAG(x) _Generic((x), int: coh_generic_tag, double: coh_generic_tag_d)(x)
void coherency_test(void) {
    int n = COH_D;
    int vla[n];
    for (int i = 0; i < n; i++)
        vla[i] = i * i;
    struct coh_s arr[] = {[2] = {.tag = 3, .val = -7},
                          [0] = {.tag = 1, .val = 5}};
    double complex z = (double)arr[0].val + (double)arr[2].val * I;
    long sum = 0;
    for (int i = 0; i < n; i++)
        sum += vla[i];
    printf("coh enum/vla: %d %ld\n", n, sum);
    printf("coh struct bitfield: %u %d %u %d\n",
           arr[0].tag, arr[0].val, arr[2].tag, arr[2].val);
    printf("coh complex-from-ints: %g %g\n", creal(z), cimag(z));
    printf("coh generic: %d %d\n", COH_TAG(n), COH_TAG((double)sum));
    printf("coh compound-literal-sum: %d\n",
           (int)(vla[1] + (int[]){10, 20, 30}[2] + COH_C));
}
