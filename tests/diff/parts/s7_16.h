/* C9911 §7.16-§7.18 stdarg/stdbool/stdint-or-atomic boundary (s7_16) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_16.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
void s7_16_stdbool(void)
{
    /* C99 §7.16p1/p3, C11 §7.18p1/p3: the four macros exist and expand
       as required (bool, true, false, __bool_true_false_are_defined). */
    printf("s7_16 stdbool defined=%d true=%d false=%d\n",
           __bool_true_false_are_defined, (int)true, (int)false);
    /* C11 §7.18p2: bool expands to _Bool -> same size */
    printf("s7_16 stdbool bsize=%d\n", (int)(sizeof(bool) == sizeof(_Bool)));

    /* §7.18p2 + §6.7.2.1: a bool object stores only 0/1; any nonzero
       scalar (int, float, pointer) collapses to 1, zero to 0. */
    bool s7_16_b1 = true;
    bool s7_16_b2 = false;
    bool s7_16_b3 = 42;
    bool s7_16_b4 = 0.0;
    bool s7_16_b5 = 0.5;
    bool s7_16_b6 = (void*)0;
    printf("s7_16 stdbool store=%d %d %d %d %d %d\n",
           (int)s7_16_b1, (int)s7_16_b2, (int)s7_16_b3,
           (int)s7_16_b4, (int)s7_16_b5, (int)s7_16_b6);

    /* §7.18p3: true==1, false==0, __bool_true_false_are_defined==1 are
       integer constants usable in #if preprocessing directives. */
#if (true == 1) && (false == 0) && (__bool_true_false_are_defined == 1)
    printf("s7_16 stdbool ppif=1\n");
#else
    printf("s7_16 stdbool ppif=0\n");
#endif

    /* §7.18p3: also usable in constant expressions. */
    _Static_assert(true == 1, "true is 1");
    _Static_assert(false == 0, "false is 0");
    _Static_assert(sizeof(bool) == sizeof(_Bool), "bool is _Bool");

    bool s7_16_arr[2] = { true, false };
    int s7_16_sum = 0;
    for (int i = 0; i < 5; i++)
        s7_16_sum += (s7_16_arr[0] ? 1 : 0);
    printf("s7_16 stdbool loop=%d\n", s7_16_sum);

    /* §7.18p4: notwithstanding §6.10.8, a program may undefine and then
       redefine bool, true and false. */
#undef bool
#undef true
#undef false
#define bool int
#define true 7
#define false 3
    bool s7_16_rb = true + false;   /* 7 + 3 = 10 */
    printf("s7_16 stdbool redef=%d\n", s7_16_rb);
#undef bool
#undef true
#undef false
}
