/* C9911 §6.9 External definitions (s6_9) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_9.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
/* §6.9.2 tentative definitions: no-initializer static objects are zero-initialized */
static int s6_9_tent_scalar;
static int s6_9_tent_arr[4];

/* §6.9.2p2: repeated tentative defs + one real def all name a single object */
int s6_9_tent_x;
int s6_9_tent_x;
int s6_9_tent_x = 42;

/* §6.9.2p2 EXAMPLE 2: an incomplete-array tentative def never completed is
   completed to a one-element array by the implicit "= 0" initializer */
int s6_9_incarr[];

/* §6.9.1p9: a parameter is a modifiable lvalue; changes to it do not affect
   the caller's argument (parameters are passed by value) */
static int s6_9_by_value(int p)
{
    p += 100;
    return p;
}

/* §6.9.1p7/p10: an array parameter is adjusted to a pointer-to-element, so
   writes through it modify the caller's storage */
static void s6_9_arr_param(int a[10])
{
    a[2] = 7;
}

/* §6.9.1p10: an argument is converted to the parameter type as if by
   assignment (9.7 -> int 9) */
static int s6_9_conv_param(int p)
{
    return p;
}

void s6_9_extdef(void)
{
    printf("tent_scalar=%d\n", s6_9_tent_scalar);
    printf("tent_arr=%d %d %d %d\n",
           s6_9_tent_arr[0], s6_9_tent_arr[1],
           s6_9_tent_arr[2], s6_9_tent_arr[3]);
    printf("tent_x=%d\n", s6_9_tent_x);

    printf("incarr_zero=%d\n", s6_9_incarr[0] == 0);
    s6_9_incarr[0] = 5;
    printf("incarr0=%d\n", s6_9_incarr[0]);

    int arg = 3;
    int got = s6_9_by_value(arg);
    printf("by_value ret=%d arg_unchanged=%d\n", got, arg == 3);

    int buf[10] = {0};
    s6_9_arr_param(buf);
    printf("arr_param buf2=%d\n", buf[2]);

    printf("conv_param=%d\n", s6_9_conv_param(9.7));
}
