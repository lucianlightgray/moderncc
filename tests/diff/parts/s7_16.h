void s7_16_stdbool(void) {

    printf("s7_16 stdbool defined=%d true=%d false=%d\n",
           __bool_true_false_are_defined, (int)true, (int)false);

    printf("s7_16 stdbool bsize=%d\n", (int)(sizeof(bool) == sizeof(_Bool)));

    bool s7_16_b1 = true;
    bool s7_16_b2 = false;
    bool s7_16_b3 = 42;
    bool s7_16_b4 = 0.0;
    bool s7_16_b5 = 0.5;
    bool s7_16_b6 = (void *)0;
    printf("s7_16 stdbool store=%d %d %d %d %d %d\n",
           (int)s7_16_b1, (int)s7_16_b2, (int)s7_16_b3,
           (int)s7_16_b4, (int)s7_16_b5, (int)s7_16_b6);

#if (true == 1) && (false == 0) && (__bool_true_false_are_defined == 1)
    printf("s7_16 stdbool ppif=1\n");
#else
    printf("s7_16 stdbool ppif=0\n");
#endif

    _Static_assert(true == 1, "true is 1");
    _Static_assert(false == 0, "false is 0");
    _Static_assert(sizeof(bool) == sizeof(_Bool), "bool is _Bool");

    bool s7_16_arr[2] = {true, false};
    int s7_16_sum = 0;
    for (int i = 0; i < 5; i++)
        s7_16_sum += (s7_16_arr[0] ? 1 : 0);
    printf("s7_16 stdbool loop=%d\n", s7_16_sum);

#undef bool
#undef true
#undef false
#define bool int
#define true 7
#define false 3
    bool s7_16_rb = true + false;
    printf("s7_16 stdbool redef=%d\n", s7_16_rb);
#undef bool
#undef true
#undef false
}
