/* C9911 §6.5.15-§6.5.17 Conditional/assignment/comma & §6.6 (s6_5_15) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_5_15.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
static int s6_5_15_calls;
static int s6_5_15_side(int v) { s6_5_15_calls++; return v; }

/* §6.5.15 conditional operator */
void s6_5_15_cond_test(void)
{
    /* p4: evaluate only the selected branch (side-effect count) */
    s6_5_15_calls = 0;
    int a = 1 ? s6_5_15_side(10) : s6_5_15_side(20);
    printf("cond_true=%d calls=%d\n", a, s6_5_15_calls);
    s6_5_15_calls = 0;
    int b = 0 ? s6_5_15_side(10) : s6_5_15_side(20);
    printf("cond_false=%d calls=%d\n", b, s6_5_15_calls);
    /* p5: usual arithmetic conversions set the result type */
    printf("cond_uac=%d\n", (1 ? -1 : 0u) > 0);            /* unsigned -> 1 */
    printf("cond_size=%d\n", (int)(sizeof(1 ? 1 : 1.0) == sizeof(double)));
    printf("cond_long=%d\n", (int)(sizeof(0 ? 1 : 1L) == sizeof(long)));
    /* p6: null-pointer-constant branch and void*, object* composite */
    int obj = 77;
    int *pp = 1 ? &obj : 0;
    printf("cond_ptr=%d\n", *pp);
    void *vp = (void *)&obj;
    printf("cond_vp=%d\n", *(int *)(0 ? vp : &obj));
}

/* §6.5.16 assignment operators */
void s6_5_16_assign_test(void)
{
    /* p3: value of an assignment is the stored value */
    int x;
    int r = (x = 42);
    printf("simple_val=%d x=%d\n", r, x);
    /* 6.5.16.1p1: _Bool <- pointer yields 1/0 by null-ness */
    int o = 1;
    _Bool bt = &o;
    _Bool bf = (int *)0;
    printf("bool_ptr=%d %d\n", bt, bf);
    /* 6.5.16.1p2: right operand converted to lvalue type */
    unsigned char uc;
    uc = 0x1234;
    printf("narrow=%d\n", uc);
    signed char sc;
    sc = 300;
    printf("wrap=%d\n", sc);
    /* 6.5.16.2p1: pointer += / -= integer */
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;
    p += 3;
    printf("ptr_add=%d\n", *p);
    p -= 2;
    printf("ptr_sub=%d\n", *p);
    /* 6.5.16.2p2: E1 op= E2 evaluates the lvalue E1 exactly once */
    int cnt[3] = {0, 0, 0};
    s6_5_15_calls = 0;
    cnt[s6_5_15_side(1)] += 5;
    printf("compound_once=%d cnt1=%d\n", s6_5_15_calls, cnt[1]);
    int y = 3;
    int cr = (y *= 4);
    printf("compound_val=%d y=%d\n", cr, y);
    /* full compound-operator set */
    int v = 24;
    v += 6;  printf("v=%d\n", v);
    v -= 4;  printf("v=%d\n", v);
    v *= 2;  printf("v=%d\n", v);
    v /= 5;  printf("v=%d\n", v);
    v %= 7;  printf("v=%d\n", v);
    v <<= 3; printf("v=%d\n", v);
    v >>= 1; printf("v=%d\n", v);
    v &= 12; printf("v=%d\n", v);
    v |= 1;  printf("v=%d\n", v);
    v ^= 5;  printf("v=%d\n", v);
}

/* §6.5.17 comma operator */
void s6_5_17_comma_test(void)
{
    /* p2: evaluate left (discard), then right; result is right's value */
    s6_5_15_calls = 0;
    int v = (s6_5_15_side(7), s6_5_15_side(9));
    printf("comma_val=%d calls=%d\n", v, s6_5_15_calls);
    int c = 0;
    int r = (c = 5, c + 1);           /* left side effect sequenced first */
    printf("comma_seq=%d\n", r);
    printf("comma_type=%d\n", (int)(sizeof(((char)1, 1L)) == sizeof(long)));
    int q = (c = 2, c + 8);
    printf("comma_arg=%d\n", q);
}

/* §6.6 constant expressions */
enum { S6_6_A = 2 * 3 + 1, S6_6_B = S6_6_A << 2, S6_6_C = sizeof(int) + 0 };

static int s6_6_obj[4] = {11, 22, 33, 44};
static int *s6_6_ptr = s6_6_obj + 2;      /* p7/p9 address constant + int */
static char *s6_6_str = "abcde" + 1;      /* p7 address constant */

void s6_6_const_test(void)
{
    /* p2/p6: integer constant expressions folded at translation time */
    printf("enum=%d %d %d\n", S6_6_A, S6_6_B, (int)(S6_6_C == sizeof(int)));
    /* p6: float cast inside an integer-constant array size */
    int arr[(int)(1.5 + 0.5) + 1];
    printf("arrsize=%d\n", (int)(sizeof arr / sizeof arr[0]));
    /* _Alignof as an ICE operand */
    int aa[_Alignof(double) >= 1 ? 3 : 1];
    printf("alignarr=%d\n", (int)(sizeof aa / sizeof aa[0]));
    /* p5: case label constant expression */
    int x = 5;
    switch (x) {
        case 2 + 3: printf("case_ok\n"); break;
        default:    printf("case_bad\n"); break;
    }
    /* p5: bit-field width constant expression */
    struct { unsigned f : 4 + 1; } bf;   /* width 5 -> holds 0..31 */
    bf.f = 20;
    printf("bf=%u\n", bf.f);
    /* p7/p9: address constants initializing static objects */
    printf("addr_off=%d\n", *s6_6_ptr);
    printf("addr_str=%s\n", s6_6_str);
}
