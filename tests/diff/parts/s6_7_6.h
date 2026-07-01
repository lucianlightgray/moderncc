/* C9911 §6.7.6 Declarators (s6_7_6) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_7_6.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
int s6_7_6_counter = 0;
int s6_7_6_getn(void) { s6_7_6_counter++; return 4; }

int s6_7_6_cbfn(void) { return 77; }
int s6_7_6_mdfunc(void) { return 55; }

void s6_7_6_apply(int cb(void))
{
    printf("apply=%d\n", cb());
}

void s6_7_6_arrparam(int a[10])
{
    printf("arrparam_sizeof_is_ptr=%d\n", (int)(sizeof a == sizeof(int *)));
}

void s6_7_6_declarators(void)
{
    /* §6.7.6 multi-declarator: one set of specifiers, three different derived
       types (pointer, array, pointer-to-function) */
    int *p, arr[3], (*fp)(void);
    arr[0] = 10; arr[1] = 20; arr[2] = 30;
    p = arr;
    fp = s6_7_6_mdfunc;
    printf("multi: %d %d %d\n",
           (int)(sizeof arr == 3 * sizeof(int)),
           p[0] + p[2],
           fp());

    /* §6.7.6.3p5: a function-type parameter is adjusted to pointer-to-function
       and is callable */
    s6_7_6_apply(s6_7_6_cbfn);

    /* §6.7.6.3p4: sizeof an array parameter yields the adjusted pointer size */
    {
        int local[10];
        s6_7_6_arrparam(local);
    }

    /* §6.7.6.2p5 / §6.7.6p4: the VLA size expression is evaluated exactly once
       when control reaches the declaration; a later sizeof does not re-evaluate
       it (counter stays 1, sizeof is stable) */
    s6_7_6_counter = 0;
    {
        int v[s6_7_6_getn()];
        int firstsz = sizeof v;
        int secondsz = sizeof v;
        printf("vla_once: %d %d %d\n",
               s6_7_6_counter,
               (int)(firstsz == 4 * (int)sizeof(int)),
               (int)(firstsz == secondsz));
    }

    /* §6.7.6.2p5: the VLA size is fixed for the array's lifetime even if the
       variable that determined it is modified afterward */
    {
        int n = 3;
        int w[n];
        n = 100;
        printf("vla_fixed: %d\n", (int)(sizeof w == 3 * sizeof(int)));
    }

    /* §6.7.6p4: multiple size expressions are each evaluated once, in order */
    s6_7_6_counter = 0;
    {
        int a = s6_7_6_getn();
        int b = s6_7_6_getn();
        int m[a][b];
        printf("vla_order: %d %d\n",
               s6_7_6_counter,
               (int)(sizeof m == (size_t)a * b * sizeof(int)));
    }
}
