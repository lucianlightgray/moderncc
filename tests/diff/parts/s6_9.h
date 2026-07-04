static int s6_9_tent_scalar;
static int s6_9_tent_arr[4];

int s6_9_tent_x;
int s6_9_tent_x;
int s6_9_tent_x = 42;

int s6_9_incarr[];

static int s6_9_by_value(int p) {
    p += 100;
    return p;
}

static void s6_9_arr_param(int a[10]) {
    a[2] = 7;
}

static int s6_9_conv_param(int p) {
    return p;
}

void s6_9_extdef(void) {
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
