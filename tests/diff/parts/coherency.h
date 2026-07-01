/* C9911 cross-feature coherency (coherency) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_coherency.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
/* Coherency: features from several parts composed in one flow, cross-checked. */
enum coh_e { COH_A, COH_B, COH_C = 10, COH_D };
struct coh_s { unsigned tag : 4; signed val : 12; };
static int coh_generic_tag(int x)   { return 1; }
static int coh_generic_tag_d(double x){ return 2; }
#define COH_TAG(x) _Generic((x), int: coh_generic_tag, double: coh_generic_tag_d)(x)
void coherency_test(void)
{
    int n = COH_D;                                  /* enum -> 11 */
    int vla[n];                                     /* VLA sized by enum */
    for (int i = 0; i < n; i++) vla[i] = i * i;
    struct coh_s arr[] = { [2] = { .tag = 3, .val = -7 },  /* designated init */
                           [0] = { .tag = 1, .val =  5 } };
    double complex z = (double)arr[0].val + (double)arr[2].val * I; /* mix ints->complex */
    long sum = 0;
    for (int i = 0; i < n; i++) sum += vla[i];      /* 0+1+4+..+100 = 385 */
    printf("coh enum/vla: %d %ld\n", n, sum);
    printf("coh struct bitfield: %u %d %u %d\n",
           arr[0].tag, arr[0].val, arr[2].tag, arr[2].val);
    printf("coh complex-from-ints: %g %g\n", creal(z), cimag(z));
    printf("coh generic: %d %d\n", COH_TAG(n), COH_TAG((double)sum));
    printf("coh compound-literal-sum: %d\n",
           (int)(vla[1] + (int[]){10,20,30}[2] + COH_C));   /* 1+30+10 = 41 */
}
