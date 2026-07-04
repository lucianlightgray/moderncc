void builtin_test(void) {
    short s;
    int i;
    long long ll;
#if GCC_MAJOR >= 3
    COMPAT_TYPE(int, int);
    COMPAT_TYPE(int, unsigned int);
    COMPAT_TYPE(int, char);
    COMPAT_TYPE(int, const int);
    COMPAT_TYPE(int, volatile int);
    COMPAT_TYPE(int *, int *);
    COMPAT_TYPE(int *, void *);
    COMPAT_TYPE(int *, const int *);
    COMPAT_TYPE(char *, unsigned char *);
    COMPAT_TYPE(char *, signed char *);
    COMPAT_TYPE(char *, char *);
    COMPAT_TYPE(char **, void *);
#endif
    printf("res1 = %d\n", __builtin_constant_p(1));
    printf("res2 = %d\n", __builtin_constant_p(1 + 2));
    printf("res3 = %d\n", __builtin_constant_p(&constant_p_var));
    printf("res4 = %d\n", __builtin_constant_p(constant_p_var));
    printf("res5 = %d\n", __builtin_constant_p(100000 / constant_p_var));
    printf("res6 = %d\n", __builtin_constant_p(i && 1));
    printf("res7 = %d\n", __builtin_constant_p("hi"));
    printf("res8 = %d\n", __builtin_constant_p(func()));
#ifndef __clang__
    printf("res10 = %d\n", __builtin_constant_p(i && 0));
    printf("res11 = %d\n", __builtin_constant_p(i * 0));
    printf("res12 = %d\n", __builtin_constant_p(i && 0 ? i : 34));
    printf("res13 = %d\n", __builtin_constant_p((1, 7)));
#else

    printf("res10 = 1\n");
    printf("res11 = 1\n");
    printf("res12 = 1\n");
    printf("res13 = 0\n");
#endif

    s = 1;
    ll = 2;
    i = __builtin_choose_expr(1 != 0, ll, s);
    printf("bce: %d\n", i);
    i = __builtin_choose_expr(1 != 1, ll, s);
    printf("bce: %d\n", i);
    i = sizeof(__builtin_choose_expr(1, ll, s));
    printf("bce: %d\n", i);
    i = sizeof(__builtin_choose_expr(0, ll, s));
    printf("bce: %d\n", i);

    {
        int cnt[18];
        unsigned long long r = 0;

        memset(cnt, 0, sizeof(cnt));
        builtin_test_bits(0, cnt);
        builtin_test_bits(0xffffffffffffffffull, cnt);
        for (i = 0; i < 64; i++)
            builtin_test_bits(1ull << i, cnt);
        for (i = 0; i < 1000; i++) {
            r = 0x5851f42d4c957f2dull * r + 0x14057b7ef767814full;
            builtin_test_bits(r, cnt);
        }
        for (i = 0; i < 18; i++)
            printf("%d %d\n", i, cnt[i]);
    }
}

#if defined _WIN32 || (defined __APPLE__ && GCC_MAJOR >= 15)
void weak_test(void) {
}
#else
extern int __attribute__((weak)) weak_f1(void);
extern int __attribute__((weak)) weak_f2(void);
extern int weak_f3(void);
extern int __attribute__((weak)) weak_v1;
extern int __attribute__((weak)) weak_v2;
extern int weak_v3;

extern int (*weak_fpa)() __attribute__((weak));
extern int __attribute__((weak)) (*weak_fpb)();
extern __attribute__((weak)) int (*weak_fpc)();

extern int weak_asm_f1(void) asm("weak_asm_f1x") __attribute((weak));
extern int __attribute((weak)) weak_asm_f2(void) asm("weak_asm_f2x");
extern int __attribute((weak)) weak_asm_f3(void) asm("weak_asm_f3x") __attribute((weak));
extern int weak_asm_v1 asm("weak_asm_v1x") __attribute((weak));
extern int __attribute((weak)) weak_asm_v2 asm("weak_asm_v2x");
extern int __attribute((weak)) weak_asm_v3(void) asm("weak_asm_v3x") __attribute((weak));

#ifndef __clang__
static const size_t dummy = 0;
extern __typeof(dummy) weak_dummy1 __attribute__((weak, alias("dummy")));
extern __typeof(dummy) __attribute__((weak, alias("dummy"))) weak_dummy2;
extern __attribute__((weak, alias("dummy"))) __typeof(dummy) weak_dummy3;
#endif

int some_lib_func(void);
int dummy_impl_of_slf(void) {
    return 444;
}
#ifndef __clang__
int some_lib_func(void) __attribute__((weak, alias("dummy_impl_of_slf")));
#endif

int weak_toolate() __attribute__((weak));
int weak_toolate() {
    return 0;
}

void __attribute__((weak)) weak_test(void) {
    printf("weak_f1=%d\n", weak_f1 ? weak_f1() : 123);
    printf("weak_f2=%d\n", weak_f2 ? weak_f2() : 123);
    printf("weak_f3=%d\n", weak_f3 ? weak_f3() : 123);
    printf("weak_v1=%d\n", &weak_v1 ? weak_v1 : 123);
    printf("weak_v2=%d\n", &weak_v2 ? weak_v2 : 123);
    printf("weak_v3=%d\n", &weak_v3 ? weak_v3 : 123);

    printf("weak_fpa=%d\n", &weak_fpa ? weak_fpa() : 123);
    printf("weak_fpb=%d\n", &weak_fpb ? weak_fpb() : 123);
    printf("weak_fpc=%d\n", &weak_fpc ? weak_fpc() : 123);

    printf("weak_asm_f1=%d\n", weak_asm_f1 != NULL);
    printf("weak_asm_f2=%d\n", weak_asm_f2 != NULL);
    printf("weak_asm_f3=%d\n", weak_asm_f3 != NULL);
    printf("weak_asm_v1=%d\n", &weak_asm_v1 != NULL);
    printf("weak_asm_v2=%d\n", &weak_asm_v2 != NULL);
    printf("weak_asm_v3=%d\n", &weak_asm_v3 != NULL);
#ifdef __clang__
    printf("some_lib_func=444\n");
#else
    printf("some_lib_func=%d\n", &some_lib_func ? some_lib_func() : 0);
#endif
}

int __attribute__((weak)) weak_f2() {
    return 222;
}
int __attribute__((weak)) weak_f3() {
    return 333;
}
int __attribute__((weak)) weak_v2 = 222;
int __attribute__((weak)) weak_v3 = 333;
#endif

void const_func(const int a) {
}

void const_warn_test(void) {
    const_func(1);
}

struct condstruct {
    int i;
};

int getme(struct condstruct *s, int i) {
    int i1 = (i == 0 ? 0 : s)->i;
    int i2 = (i == 0 ? s : 0)->i;
    int i3 = (i == 0 ? (void *)0 : s)->i;
    int i4 = (i == 0 ? s : (void *)0)->i;
    return i1 + i2 + i3 + i4;
}

struct global_data {
    int a[40];
    int *b[40];
};

struct global_data global_data;

int global_data_getstuff(int *, int);

void global_data_callit(int i) {
    *global_data.b[i] = global_data_getstuff(global_data.b[i], 1);
}

int global_data_getstuff(int *p, int i) {
    return *p + i;
}

void global_data_test(void) {
    global_data.a[0] = 42;
    global_data.b[0] = &global_data.a[0];
    global_data_callit(0);
    printf("%d\n", global_data.a[0]);
}

struct cmpcmpS {
    unsigned char fill : 3;
    unsigned char b1 : 1;
    unsigned char b2 : 1;
    unsigned char fill2 : 3;
};

int glob1, glob2, glob3;

void compare_comparisons(struct cmpcmpS *s) {
    if (s->b1 != (glob1 == glob2) || (s->b2 != (glob1 == glob3)))
        printf("comparing comparisons broken\n");
}

void cmp_comparison_test(void) {
    struct cmpcmpS s;
    s.b1 = 1;
    glob1 = 42;
    glob2 = 42;
    s.b2 = 0;
    glob3 = 43;
    compare_comparisons(&s);
}

int fcompare(double a, double b, int code) {
    switch (code) {
    case 0:
        return a == b;
    case 1:
        return a != b;
    case 2:
        return a < b;
    case 3:
        return a >= b;
    case 4:
        return a > b;
    case 5:
        return a <= b;
    }
    return 0;
}

void math_cmp_test(void) {
    double nan = 0.0 / 0.0;
    double one = 1.0;
    double two = 2.0;
    int comp = 0;
    int v;
#define bug(a, b, op, iop, part) printf("Test broken: %s %s %s %s %d\n", #a, #b, #op, #iop, part)

#define FCMP(a, b, op, iop, code) \
    if (fcompare(a, b, code))     \
        bug(a, b, op, iop, 1);    \
    if (a op b)                   \
        bug(a, b, op, iop, 2);    \
    if (a iop b)                  \
        ;                         \
    else                          \
        bug(a, b, op, iop, 3);    \
    if ((a op b) || comp)         \
        bug(a, b, op, iop, 4);    \
    if ((a iop b) || comp)        \
        ;                         \
    else                          \
        bug(a, b, op, iop, 5);    \
    if (v = !(a op b), !v)        \
        bug(a, b, op, iop, 7);

    FCMP(nan, nan, ==, !=, 0);
    FCMP(one, two, ==, !=, 0);
    FCMP(one, one, !=, ==, 1);

    if (!fcompare(nan, nan, 1))
        bug(nan, nan, !=, ==, 6);

    FCMP(two, one, <, >=, 2);
    FCMP(one, two, >=, <, 3);
    FCMP(one, two, >, <=, 4);
    FCMP(two, one, <=, >, 5);

    FCMP(nan, nan, <, !=, 2);
    FCMP(nan, nan, >=, !=, 3);
    FCMP(nan, nan, >, !=, 4);
    FCMP(nan, nan, <=, !=, 5);
}

double get100() {
    return 100.0;
}

void callsave_test(void) {
    int i, s;
    double *d;
    double t;
    s = sizeof(double);
    printf("callsavetest: %d\n", s);
    d = alloca(sizeof(double));
    d[0] = 10.0;

    i = d[0] > get100();
    printf("%d\n", i);
}

void bfa3(ptrdiff_t str_offset) {
    printf("bfa3: %s\n", (char *)__builtin_frame_address(3) + str_offset);
}
void bfa2(ptrdiff_t str_offset) {
    printf("bfa2: %s\n", (char *)__builtin_frame_address(2) + str_offset);
    bfa3(str_offset);
}
void bfa1(ptrdiff_t str_offset) {
    printf("bfa1: %s\n", (char *)__builtin_frame_address(1) + str_offset);
    bfa2(str_offset);
}

void builtin_frame_address_test(void) {

#ifndef __arm__
    char str[] = "__builtin_frame_address";
    char *fp0 = __builtin_frame_address(0);

    printf("str: %s\n", str);
#ifndef __riscv
    bfa1(str - fp0);
#endif
#endif
}

char via_volatile(char i) {
    char volatile vi;
    vi = i;
    return vi;
}

void volatile_test(void) {
    if (via_volatile(42) != 42)
        printf(" broken\n");
    else
        printf(" ok\n");
}

struct __attribute__((__packed__)) Spacked {
    char a;
    short b;
    int c;
};
struct Spacked spacked;
typedef struct __attribute__((__packed__)) {
    char a;
    short b;
    int c;
} Spacked2;
Spacked2 spacked2;
typedef struct Spacked3_s {
    char a;
    short b;
    int c;
} __attribute__((__packed__)) Spacked3;
Spacked3 spacked3;
struct gate_struct64 {
    unsigned short offset_low;
    unsigned short segment;
    unsigned ist : 3, zero0 : 5, type : 5, dpl : 2, p : 1;
    unsigned short offset_middle;
    unsigned offset_high;
    unsigned zero1;
} __attribute__((packed));
typedef struct gate_struct64 gate_desc;
gate_desc a_gate_desc;
void attrib_test(void) {
#ifndef _WIN32
    printf("attr: %d %d %d %d\n", sizeof(struct Spacked),
           sizeof(spacked), sizeof(Spacked2), sizeof(spacked2));
    printf("attr: %d %d\n", sizeof(Spacked3), sizeof(spacked3));
    printf("attr: %d %d\n", sizeof(gate_desc), sizeof(a_gate_desc));
#endif
}
extern __attribute__((__unused__)) char *__attribute__((__unused__)) *
    strange_attrib_placement(void);

void *__attribute__((__unused__)) get_void_ptr(void *a) {
    return a;
}

static inline int __get_order(unsigned long long size) {
    int order;
    size -= 0xffff880000000000ULL;
    {
        struct S {
            int i : 1;
        } s;
    }
    order = size;
    return order;
}

int force_get_order(unsigned long s) {
    return __get_order(s);
}

#define pv(m) printf(sizeof(s->m + 0) == 8 ? "%016llx\n" : "%02x\n", s->m)

void bounds_check1_test(void) {
    struct s {
        int x;
        long long y;
    } _s, *s = &_s;
    s->x = 10;
    s->y = 20;
    pv(x);
    pv(y);
}

void map_add(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    printf("%d %d %d %d %d %d %d %d %d\n", a, b, c, d, e, f, g, h, i);
}

void func_arg_test(void) {
    int a = 0;
    int b = 1;
    map_add(0, 1, 2, 3, 4, 5, 6, 7, a && b);
}

#define CORRECT_CR_HANDLING

#ifdef __MCC__

#endif

#define mcc_test()

void whitespace_test(void) {
    char *str;
    int mcc_test = 1;

#if 1
    pri\
ntf("whitspace:\n");
#endif
    pf("N=%d\n", 2);

#ifdef CORRECT_CR_HANDLING
    pri\
ntf("aaa=%d\n", 3);
#endif

    pri\
\
ntf("min=%d\n", 4);

#ifdef ACCEPT_LF_IN_STRINGS
    printf("len1=%d\n", strlen("
"));
#ifdef CORRECT_CR_HANDLING
    str = "
";
    printf("len1=%d str[0]=%d\n", strlen(str), str[0]);
#endif
    printf("len1=%d\n", strlen("
a
"));
#else
    printf("len1=1\nlen1=1 str[0]=10\nlen1=3\n");
#endif

#ifdef __LINE__
    printf("__LINE__ defined\n");
#endif

#if 0

    printf("__LINE__=%d __FILE__=%s\n", __LINE__, __FILE__);
#line 1111
    printf("__LINE__=%d __FILE__=%s\n", __LINE__, __FILE__);
#line 2222 "test"
    printf("__LINE__=%d __FILE__=%s\n", __LINE__, __FILE__);
#endif

    printf("\\
"12\\
063\\
n 456\"\n");

    printf ("%d\n",
#if 1
	    mcc_test
#endif
            );
}
