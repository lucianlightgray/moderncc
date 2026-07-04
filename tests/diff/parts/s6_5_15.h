static int s6_5_15_calls;
static int s6_5_15_side(int v) {
    s6_5_15_calls++;
    return v;
}

void s6_5_15_cond_test(void) {

    s6_5_15_calls = 0;
    int a = 1 ? s6_5_15_side(10) : s6_5_15_side(20);
    printf("cond_true=%d calls=%d\n", a, s6_5_15_calls);
    s6_5_15_calls = 0;
    int b = 0 ? s6_5_15_side(10) : s6_5_15_side(20);
    printf("cond_false=%d calls=%d\n", b, s6_5_15_calls);

    printf("cond_uac=%d\n", (1 ? -1 : 0u) > 0);
    printf("cond_size=%d\n", (int)(sizeof(1 ? 1 : 1.0) == sizeof(double)));
    printf("cond_long=%d\n", (int)(sizeof(0 ? 1 : 1L) == sizeof(long)));

    int obj = 77;
    int *pp = 1 ? &obj : 0;
    printf("cond_ptr=%d\n", *pp);
    void *vp = (void *)&obj;
    printf("cond_vp=%d\n", *(int *)(0 ? vp : &obj));
}

void s6_5_16_assign_test(void) {

    int x;
    int r = (x = 42);
    printf("simple_val=%d x=%d\n", r, x);

    int o = 1;
    _Bool bt = &o;
    _Bool bf = (int *)0;
    printf("bool_ptr=%d %d\n", bt, bf);

    unsigned char uc;
    uc = 0x1234;
    printf("narrow=%d\n", uc);
    signed char sc;
    sc = 300;
    printf("wrap=%d\n", sc);

    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;
    p += 3;
    printf("ptr_add=%d\n", *p);
    p -= 2;
    printf("ptr_sub=%d\n", *p);

    int cnt[3] = {0, 0, 0};
    s6_5_15_calls = 0;
    cnt[s6_5_15_side(1)] += 5;
    printf("compound_once=%d cnt1=%d\n", s6_5_15_calls, cnt[1]);
    int y = 3;
    int cr = (y *= 4);
    printf("compound_val=%d y=%d\n", cr, y);

    int v = 24;
    v += 6;
    printf("v=%d\n", v);
    v -= 4;
    printf("v=%d\n", v);
    v *= 2;
    printf("v=%d\n", v);
    v /= 5;
    printf("v=%d\n", v);
    v %= 7;
    printf("v=%d\n", v);
    v <<= 3;
    printf("v=%d\n", v);
    v >>= 1;
    printf("v=%d\n", v);
    v &= 12;
    printf("v=%d\n", v);
    v |= 1;
    printf("v=%d\n", v);
    v ^= 5;
    printf("v=%d\n", v);
}

void s6_5_17_comma_test(void) {

    s6_5_15_calls = 0;
    int v = (s6_5_15_side(7), s6_5_15_side(9));
    printf("comma_val=%d calls=%d\n", v, s6_5_15_calls);
    int c = 0;
    int r = (c = 5, c + 1);
    printf("comma_seq=%d\n", r);
    printf("comma_type=%d\n", (int)(sizeof(((char)1, 1L)) == sizeof(long)));
    int q = (c = 2, c + 8);
    printf("comma_arg=%d\n", q);
}

enum { S6_6_A = 2 * 3 + 1,
       S6_6_B = S6_6_A << 2,
       S6_6_C = sizeof(int) + 0 };

static int s6_6_obj[4] = {11, 22, 33, 44};
static int *s6_6_ptr = s6_6_obj + 2;
static char *s6_6_str = "abcde" + 1;

void s6_6_const_test(void) {

    printf("enum=%d %d %d\n", S6_6_A, S6_6_B, (int)(S6_6_C == sizeof(int)));

    int arr[(int)(1.5 + 0.5) + 1];
    printf("arrsize=%d\n", (int)(sizeof arr / sizeof arr[0]));

    int aa[_Alignof(double) >= 1 ? 3 : 1];
    printf("alignarr=%d\n", (int)(sizeof aa / sizeof aa[0]));

    int x = 5;
    switch (x) {
    case 2 + 3:
        printf("case_ok\n");
        break;
    default:
        printf("case_bad\n");
        break;
    }

    struct {
        unsigned f : 4 + 1;
    } bf;
    bf.f = 20;
    printf("bf=%u\n", bf.f);

    printf("addr_off=%d\n", *s6_6_ptr);
    printf("addr_str=%s\n", s6_6_str);
}
