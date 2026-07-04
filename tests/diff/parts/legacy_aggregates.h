void optimize_out_test(void) {
    int i = 0 ? undefined_function() : defined_function();
    printf("oo:%d\n", i);
    int j = 1 ? defined_function() : undefined_function();
    printf("oo:%d\n", j);
    if (0)
        printf("oo:%d\n", undefined_function());
    else
        printf("oo:%d\n", defined_function());
    if (1)
        printf("oo:%d\n", defined_function());
    else
        printf("oo:%d\n", undefined_function());
    while (1) {
        printf("oow:%d\n", defined_function());
        break;
        printf("oow:%d\n", undefined_function());
    }
    j = 1;

    switch (j)
    case 1:
        break;
    printf("oos:%d\n", defined_function());

    while (1)
        break;
    printf("ool1:%d\n", defined_function());

    do
        break;
    while (1);
    printf("ool2:%d\n", defined_function());
    for (;;)
        break;
    printf("ool3:%d\n", defined_function());

    while (1) {
        {
            break;
        }
        printf("ool4:%d\n", undefined_function());
    }
    j = 1;
    while (j) {
        if (j == 0)
            break;
        printf("ool5:%d\n", defined_function());
        j--;
    }

    j = 1;
    while (j) {
        if (1)
            j--;
        else
        breakhere:
            break;
        printf("ool6:%d\n", defined_function());
        goto breakhere;
    }
    j = 1;
    while (j) {
        j--;
        continue;
        printf("ool7:%d\n", undefined_function());
    }

    i = 0 && undefined_function();
    i = defined_function() && 0 && undefined_function();
    if (0 && undefined_function())
        undefined_function();
    if (defined_function() && 0)
        undefined_function();
    if (0 && 0)
        undefined_function();
    if (defined_function() && 0 && undefined_function())
        undefined_function();

    i = 1 || undefined_function();
    i = defined_function() || 1 || undefined_function();
    if (1 || undefined_function())
        ;
    else
        undefined_function();
    if (defined_function() || 1)
        ;
    else
        undefined_function();
    if (1 || 1)
        ;
    else
        undefined_function();
    if (defined_function() || 1 || undefined_function())
        ;
    else
        undefined_function();

    if (defined_function() && 0)
        refer_to_undefined();

    if (0) {
        (void)sizeof(({
            do {
            } while (0);
            0;
        }));
        undefined_function();
    }

    if (0) {
        switch (defined_function()) {
        case 0:
            undefined_function();
            break;
        default:
            undefined_function();
            break;
        }
    }

    if (1)
        return;
    printf("oor:%d\n", undefined_function());
}

int defined_function(void) {
    static int i = 40;
    return i++;
}

static int tab_reinit[];
static int tab_reinit[10];

static int tentative_ar[];
static int tentative_ar[] = {1, 2, 3};

int cinit1;
int cinit1;
int cinit1 = 0;
int *cinit2 = (int[]){3, 2, 1};
uintptr_t cinit3 = (uintptr_t)"AA";
char const *const cinit8[] = {[0 ... 1] = "BB", [2 ... 4] = "CC"};
void *cinit52 = &(void *){(void *)52};

#if __MCC__ || __GNUC__ >= 6
int cinit4 = (int){44};
void *cinit51 = (void *){(void *)51};
struct _c6 {
    int a, b;
} cinit6 = (struct _c6){61, 62}, *cinit7 = &(struct _c6){71, 72};
#else
int cinit4 = 44;
void *cinit51 = (void *)51;
struct _c6 {
    int a, b;
} cinit6 = {61, 62}, cinit70 = {71, 72}, *cinit7 = &cinit70;
#endif

void compound_literal_test(void) {
    int *p, i;
    char *q, *q3;

    printf("cinit3 : %s\n", cinit3);
    printf("cinit4 : %d\n", cinit4);
    printf("cinit51 : %d\n", (int)cinit51);
    printf("cinit52 : %d\n", *(int *)cinit52);
    printf("cinit6 : %d %d\n", cinit6.a, cinit6.b);
    printf("cinit7 : %d %d\n", cinit7->a, cinit7->b);
    printf("cinit8 : %s %s %s %s %s\n", cinit8[0], cinit8[1], cinit8[2], cinit8[3], cinit8[4]);

    p = (int[]){1, 2, 3};
    for (i = 0; i < 3; i++)
        printf(" %d", p[i]);
    printf("\n");

    for (i = 0; i < 3; i++)
        printf("%d", cinit2[i]);
    printf("\n");

    q = "tralala1";
    printf("q1=%s\n", q);

    q = (char *){"tralala2"};
    printf("q2=%s\n", q);

    q3 = (char *){q};
    printf("q3=%s\n", q3);

    q = (char[]){"tralala3"};
    printf("q4=%s\n", q);

#ifdef ALL_ISOC99
    p = (int[]){1, 2, cinit1 + 3};
    for (i = 0; i < 3; i++)
        printf(" %d", p[i]);
    printf("\n");

    for (i = 0; i < 3; i++) {
        p = (int[]){1, 2, 4 + i};
        printf("%d %d %d\n",
               p[0],
               p[1],
               p[2]);
    }
#endif
}

#if __MCC__

kr_func1(a, b) {
    return a + b;
}

int kr_func2(a, b) {
    return a + b;
}

kr_test() {
    printf("func1=%d\n", kr_func1(3, 4));
    printf("func2=%d\n", kr_func2(3, 4));
    return 0;
}

char invalid_function_def()[] {
    return 0;
}

#else
#define kr_test() printf("func1=7\nfunc2=7\n")
#endif

void num(int n) {
    char *tab, *p;
    tab = (char *)malloc(20);
    p = tab;
    while (1) {
        *p = 48 + (n % 10);
        p++;
        n = n / 10;
        if (n == 0)
            break;
    }
    while (p != tab) {
        p--;
        printf("%c", *p);
    }
    printf("\n");
    free(tab);
}

struct structa1 {
    int f1;
    char f2;
};

void struct_assign_test1(struct structa1 s1, int t, float f) {
    printf("%d %d %d %f\n", s1.f1, s1.f2, t, f);
}

struct structa1 struct_assign_test2(struct structa1 s1, int t) {
    s1.f1 += t;
    s1.f2 -= t;
    return s1;
}

void struct_assign_test(void) {
    struct S {
        struct structa1 lsta1, lsta2;
        int i;
    } s = {{1, 2}, {3, 4}}, *ps;

    ps = &s;
    ps->i = 4;

    struct_assign_test1(ps->lsta2, 3, 4.5);
    printf("before call: %d %d\n", s.lsta2.f1, s.lsta2.f2);
    ps->lsta2 = struct_assign_test2(ps->lsta2, ps->i);
    printf("after call: %d %d\n", ps->lsta2.f1, ps->lsta2.f2);

    static struct {
        void (*elem)();
    } t[] = {

        {struct_assign_test}};
    printf("%d\n", struct_assign_test == t[0].elem);

    s.lsta1 = s.lsta2 = struct_assign_test2(s.lsta1, 1);
    printf("%d %d\n", s.lsta1.f1, s.lsta1.f2);
}

void cast1(char a, short b, unsigned char c, unsigned short d) {
    printf("%d %d %d %d\n", a, b, c, d);
}

char bcast;
short scast;

void cast_test() {
    int a;
    char c;
    char tab[10];
    unsigned b, d;
    short s;
    char *p = NULL;
    unsigned long ul = 0x80000000UL;
    p -= 0x700000000042;

    a = 0xfffff;
    cast1(a, a, a, a);
    a = 0xffffe;
    printf("%d %d %d %d\n",
           (char)(a + 1),
           (short)(a + 1),
           (unsigned char)(a + 1),
           (unsigned short)(a + 1));
    printf("%d %d %d %d\n",
           (char)0xfffff,
           (short)0xfffff,
           (unsigned char)0xfffff,
           (unsigned short)0xfffff);

    a = (bcast = 128) + 1;
    printf("%d\n", a);
    a = (scast = 65536) + 1;
    printf("%d\n", a);

    printf("sizeof(c) = %d, sizeof((int)c) = %d\n", sizeof(c), sizeof((int)c));

    b = 0xf000;
    d = (short)b;
    printf("((unsigned)(short)0x%08x) = 0x%08x\n", b, d);
    b = 0xf0f0;
    d = (char)b;
    printf("((unsigned)(char)0x%08x) = 0x%08x\n", b, d);

    c = 0;
    tab[1] = 2;
    tab[c] = 1;
    printf("%d %d\n", tab[0], tab[1]);

    printf("sizeof(+(char)'a') = %d\n", sizeof(+(char)'a'));
    printf("sizeof(-(char)'a') = %d\n", sizeof(-(char)'a'));
    printf("sizeof(~(char)'a') = %d\n", sizeof(-(char)'a'));

#if CC_NAME != CC_clang

    printf("%d %d %ld %ld %lld %lld\n",
           (int)p, (unsigned int)p,
           (long)p, (unsigned long)p,
           (long long)p, (unsigned long long)p);
#endif

    printf("%p %p %p %p\n",
           (void *)a, (void *)b, (void *)c, (void *)d);

    printf("0x%lx\n", (unsigned long)(int)ul);
}

struct structinit1 {
    int f1;
    char f2;
    short f3;
    int farray[3];
};

int sinit1 = 2;
int sinit2 = {3};
int sinit3[3] = {
    1,
    2,
    {3},
};
int sinit4[3][2] = {{1, 2}, {3, 4}, {5, 6}};
int sinit5[3][2] = {1, 2, 3, 4, 5, 6};
int sinit6[] = {1, 2, 3};
int sinit7[] = {[2] = 3, [0] = 1, 2};
char sinit8[] = "hello"
                "trala";

struct structinit1 sinit9 = {1, 2, 3};
struct structinit1 sinit10 = {.f2 = 2, 3, .f1 = 1};
struct structinit1 sinit11 = {
    .f2 = 2,
    3,
    .f1 = 1,
#ifdef ALL_ISOC99
    .farray[0] = 10,
    .farray[1] = 11,
    .farray[2] = 12,
#endif
};

char *sinit12 = "hello world";
char *sinit13[] = {
    "test1",
    "test2",
    "test3",
};
char sinit14[10] = {"abc"};
int sinit15[3] = {sizeof(sinit15), 1, 2};

struct {
    int a[3], b;
} sinit16[] = {{1}, 2};

struct bar {
    char *s;
    int len;
} sinit17[] = {
    "a1", 4,
    "a2", 1};

int sinit18[10] = {
    [2 ... 5] = 20,
    2,
    [8] = 10,
};

struct complexinit0 {
    int a;
    int b;
};

struct complexinit {
    int a;
    const struct complexinit0 *b;
};

const static struct complexinit cix[] = {
    [0] = {
        .a = 2000,
        .b = (const struct complexinit0[]){
            {2001, 2002},
            {2003, 2003},
            {}}}};

struct complexinit2 {
    int a;
    int b[];
};

struct complexinit2 cix20;

struct complexinit2 cix21 = {
    .a = 3000,
    .b = {3001, 3002, 3003}};

struct complexinit2 cix22 = {
    .a = 4000,
    .b = {4001, 4002, 4003, 4004, 4005, 4006}};

typedef int arrtype1[];
arrtype1 sinit19;
arrtype1 sinit20;
arrtype1 sinit19 = {1};
arrtype1 sinit20 = {2, 3};
typedef int arrtype2[3];
arrtype2 sinit21;
arrtype2 sinit22;
arrtype2 sinit21 = {4};
arrtype2 sinit22 = {5, 6, 7};

int sinit23[2] = {"astring" ? sizeof("astring") : -1,
                  &sinit23 ? 42 : -1};

int sinit24 = 2 || 1 / 0;

struct bf_SS {
    unsigned int bit : 1, bits31 : 31;
};
struct bf_SS bf_init = {.bit = 1};
struct bfn_SS {
    int a, b;
    struct bf_SS c;
    int d, e;
};
struct bfn_SS bfn_init = {.c.bit = 1};
struct bfa_SS {
    int a, b;
    struct bf_SS c[3];
    int d, e;
};
struct bfa_SS bfa_init = {.c[1].bit = 1};
struct bf_SS bfaa_init[3] = {[1].bit = 1};
struct bf_SS bfaa_vinit[] = {[2].bit = 1};
struct b2_SS {
    long long int field : 52;
    long long int pad : 12;
};
struct b2_SS bf_init2 = {0xFFF000FFF000FLL, 0x123};

extern int external_inited = 42;

void init_test(void) {
    int linit1 = 2;
    int linit2 = {3};
    int linit4[3][2] = {{1, 2}, {3, 4}, {5, 6}};
    int linit6[] = {1, 2, 3};
    int i, j;
    char linit8[] = "hello"
                    "trala";
    int linit12[10] = {1, 2};
    int linit13[10] = {
        1,
        2,
        [7] = 3,
        [3] = 4,
    };
    char linit14[10] = "abc";
    int linit15[10] = {
        linit1,
        linit1 + 1,
        [6] = linit1 + 2,
    };
    struct linit16 {
        int a1, a2, a3, a4;
    } linit16 = {1, .a3 = 2};
    int linit17 = sizeof(linit17);
    int zero = 0;

    int linit18[2] = {&zero ? 1 : -1, zero ? -1 : 1};
    struct bf_SS bf_finit = {.bit = 1};
    struct bfn_SS bfn_finit = {.c.bit = 1};
    struct bfa_SS bfa_finit = {.c[1].bit = 1};
    struct bf_SS bfaa_finit[3] = {[1].bit = 1};
    struct bf_SS bfaa_fvinit[] = {[2].bit = 1};
    struct b2_SS bf_finit2 = {0xFFF000FFF000FLL, 0x123};

    printf("sinit1=%d\n", sinit1);
    printf("sinit2=%d\n", sinit2);
    printf("sinit3=%d %d %d %d\n",
           sizeof(sinit3),
           sinit3[0],
           sinit3[1],
           sinit3[2]);
    printf("sinit6=%d\n", sizeof(sinit6));
    printf("sinit7=%d %d %d %d\n",
           sizeof(sinit7),
           sinit7[0],
           sinit7[1],
           sinit7[2]);
    printf("sinit8=%s\n", sinit8);
    printf("sinit9=%d %d %d\n",
           sinit9.f1,
           sinit9.f2,
           sinit9.f3);
    printf("sinit10=%d %d %d\n",
           sinit10.f1,
           sinit10.f2,
           sinit10.f3);
    printf("sinit11=%d %d %d %d %d %d\n",
           sinit11.f1,
           sinit11.f2,
           sinit11.f3,
           sinit11.farray[0],
           sinit11.farray[1],
           sinit11.farray[2]);

    for (i = 0; i < 3; i++)
        for (j = 0; j < 2; j++)
            printf("[%d][%d] = %d %d %d\n",
                   i, j, sinit4[i][j], sinit5[i][j], linit4[i][j]);
    printf("linit1=%d\n", linit1);
    printf("linit2=%d\n", linit2);
    printf("linit6=%d\n", sizeof(linit6));
    printf("linit8=%d %s\n", sizeof(linit8), linit8);

    printf("sinit12=%s\n", sinit12);
    printf("sinit13=%d %s %s %s\n",
           sizeof(sinit13),
           sinit13[0],
           sinit13[1],
           sinit13[2]);
    printf("sinit14=%s\n", sinit14);

    for (i = 0; i < 10; i++)
        printf(" %d", linit12[i]);
    printf("\n");
    for (i = 0; i < 10; i++)
        printf(" %d", linit13[i]);
    printf("\n");
    for (i = 0; i < 10; i++)
        printf(" %d", linit14[i]);
    printf("\n");
    for (i = 0; i < 10; i++)
        printf(" %d", linit15[i]);
    printf("\n");
    printf("%d %d %d %d\n",
           linit16.a1,
           linit16.a2,
           linit16.a3,
           linit16.a4);

    printf("linit17=%d\n", linit17);
    printf("sinit15=%d\n", sinit15[0]);
    printf("sinit16=%d %d\n", sinit16[0].a[0], sinit16[1].a[0]);
    printf("sinit17=%s %d %s %d\n",
           sinit17[0].s, sinit17[0].len,
           sinit17[1].s, sinit17[1].len);
    for (i = 0; i < 10; i++)
        printf("%x ", sinit18[i]);
    printf("\n");

    printf("cix: %d %d %d %d %d %d %d\n",
           cix[0].a,
           cix[0].b[0].a, cix[0].b[0].b,
           cix[0].b[1].a, cix[0].b[1].b,
           cix[0].b[2].a, cix[0].b[2].b);
    printf("cix2: %d %d\n", cix21.b[2], cix22.b[5]);
    printf("sizeof cix20 %d, cix21 %d, sizeof cix22 %d\n", sizeof cix20, sizeof cix21, sizeof cix22);

    printf("arrtype1: %d %d %d\n", sinit19[0], sinit20[0], sinit20[1]);
    printf("arrtype2: %d %d\n", sizeof(sinit19), sizeof(sinit20));
    printf("arrtype3: %d %d %d\n", sinit21[0], sinit21[1], sinit21[2]);
    printf("arrtype4: %d %d %d\n", sinit22[0], sinit22[1], sinit22[2]);
    printf("arrtype5: %d %d\n", sizeof(sinit21), sizeof(sinit22));
    printf("arrtype6: %d\n", sizeof(arrtype2));

    printf("sinit23= %d %d\n", sinit23[0], sinit23[1]);
    printf("sinit24=%d\n", sinit24);
    printf("linit18= %d %d\n", linit18[0], linit18[1]);
    printf("bf1: %u %u\n", bf_init.bit, bf_init.bits31);
    printf("bf2: %u %u\n", bf_finit.bit, bf_finit.bits31);
    printf("bf3: %u %u\n", bfn_init.c.bit, bfn_init.c.bits31);
    printf("bf4: %u %u\n", bfn_finit.c.bit, bfn_finit.c.bits31);
    for (i = 0; i < 3; i++)
        printf("bf5[%d]: %u %u\n", i, bfa_init.c[i].bit, bfa_init.c[i].bits31);
    for (i = 0; i < 3; i++)
        printf("bf6[%d]: %u %u\n", i, bfa_finit.c[i].bit, bfa_finit.c[i].bits31);
    for (i = 0; i < 3; i++)
        printf("bf7[%d]: %u %u\n", i, bfaa_init[i].bit, bfaa_init[i].bits31);
    for (i = 0; i < 3; i++)
        printf("bf8[%d]: %u %u\n", i, bfaa_finit[i].bit, bfaa_finit[i].bits31);
    for (i = 0; i < 3; i++)
        printf("bf9[%d]: %u %u\n", i, bfaa_vinit[i].bit, bfaa_vinit[i].bits31);
    for (i = 0; i < 3; i++)
        printf("bf10[%d]: %u %u\n", i, bfaa_fvinit[i].bit, bfaa_fvinit[i].bits31);
}

void switch_uc(unsigned char uc) {
    switch (uc) {
    case 0xfb ... 0xfe:
        printf("ucsw:1\n");
        break;
    case 0xff:
        printf("ucsw:2\n");
        break;
    case 0 ... 5:
        printf("ucsw:3\n");
        break;
    default:
        printf("ucsw: broken!\n");
    }
}

void switch_sc(signed char sc) {
    switch (sc) {
    case -5 ... - 2:
        printf("scsw:1\n");
        break;
    case -1:
        printf("scsw:2\n");
        break;
    case 0 ... 5:
        printf("scsw:3\n");
        break;
    default:
        printf("scsw: broken!\n");
    }
}

void switch_test() {
    int i;
    unsigned long long ull;
    long long ll;

    for (i = 0; i < 15; i++) {
        switch (i) {
        case 0:
        case 1:
            printf("a");
            break;
        default:
            printf("%d", i);
            break;
        case 8 ... 12:
            printf("c");
            break;
        case 3:
            printf("b");
            break;
        case 0xc33c6b9fU:
        case 0x7c9eeeb9U:
            break;
        }
    }
    printf("\n");

    for (i = 1; i <= 5; i++) {
        ull = (unsigned long long)i << 61;
        switch (ull) {
        case 1ULL << 61:
            printf("ullsw:1\n");
            break;
        case 2ULL << 61:
            printf("ullsw:2\n");
            break;
        case 3ULL << 61:
            printf("ullsw:3\n");
            break;
        case 4ULL << 61:
            printf("ullsw:4\n");
            break;
        case 5ULL << 61:
            printf("ullsw:5\n");
            break;
        default:
            printf("ullsw: broken!\n");
        }
    }

    for (i = 1; i <= 5; i++) {
        ll = (long long)i << 61;
        switch (ll) {
        case 1LL << 61:
            printf("llsw:1\n");
            break;
        case 2LL << 61:
            printf("llsw:2\n");
            break;
        case 3LL << 61:
            printf("llsw:3\n");
            break;
        case 4LL << 61:
            printf("llsw:4\n");
            break;
        case 5LL << 61:
            printf("llsw:5\n");
            break;
        default:
            printf("llsw: broken!\n");
        }
    }

    for (i = -5; i <= 5; i++) {
        switch_uc((unsigned char)i);
    }

    for (i = -5; i <= 5; i++) {
        switch_sc((signed char)i);
    }
}

void c99_bool_test(void) {
#ifdef BOOL_ISOC99
    int a;
    _Bool b, b2;

    printf("sizeof(_Bool) = %d\n", sizeof(_Bool));
    a = 3;
    printf("cast: %d %d %d\n", (_Bool)10, (_Bool)0, (_Bool)a);
    b = 3;
    printf("b = %d\n", b);
    b++;
    printf("b = %d\n", b);
    b2 = 0;
    printf("sizeof(x ? _Bool : _Bool) = %d (should be sizeof int)\n",
           sizeof((volatile int)a ? b : b2));
#endif
}

void bitfield_test(void) {
    int a;
    short sa;
    unsigned char ca;
    struct sbf1 {
        int f1 : 3;
        int : 2;
        int f2 : 1;
        int : 0;
        int f3 : 5;
        int f4 : 7;
        unsigned int f5 : 7;
    } st1;
    printf("sizeof(st1) = %d\n", sizeof(st1));

    st1.f1 = 3;
    st1.f2 = 1;
    st1.f3 = 15;
    a = 120;
    st1.f4 = a;
    st1.f5 = a;
    st1.f5++;
    printf("%d %d %d %d %d\n",
           st1.f1, st1.f2, st1.f3, st1.f4, st1.f5);
    sa = st1.f5;
    ca = st1.f5;
    printf("%d %d\n", sa, ca);

    st1.f1 = 7;
    if (st1.f1 == -1)
        printf("st1.f1 == -1\n");
    else
        printf("st1.f1 != -1\n");
    if (st1.f2 == -1)
        printf("st1.f2 == -1\n");
    else
        printf("st1.f2 != -1\n");

    struct sbf2 {
        long long f1 : 45;
        long long : 2;
        long long f2 : 35;
        unsigned long long f3 : 38;
    } st2;
    st2.f1 = 0x123456789ULL;
    a = 120;
    st2.f2 = (long long)a << 25;
    st2.f3 = a;
    st2.f2++;
    printf("%lld %lld %lld\n", st2.f1, st2.f2, st2.f3);

#if 0
    Disabled for now until further clarification re GCC compatibility
    struct sbf3 {
        int f1 : 7;
        int f2 : 1;
        char f3;
        int f4 : 8;
        int f5 : 1;
        int f6 : 16;
    } st3;
    printf("sizeof(st3) = %d\n", sizeof(st3));
#endif

    struct sbf4 {
        int x : 31;
        char y : 2;
    } st4;
    st4.y = 1;
    printf("st4.y == %d\n", st4.y);
    struct sbf5 {
        int a;
        char b;
        int x : 12, y : 4, : 0, : 4, z : 3;
        char c;
    } st5 = {1, 2, 3, 4, -3, 6};
    printf("st5 = %d %d %d %d %d %d\n", st5.a, st5.b, st5.x, st5.y, st5.z, st5.c);
    struct sbf6 {
        short x : 12;
        unsigned char y : 2;
    } st6;
    st6.y = 1;
    printf("st6.y == %d\n", st6.y);
}

#ifdef __x86_64__
#define FLOAT_FMT "%f\n"
#else

#define FLOAT_FMT "%.5f\n"
#endif

double strtod(const char *nptr, char **endptr);

#if defined(_WIN32)
float strtof(const char *nptr, char **endptr) {
    return (float)strtod(nptr, endptr);
}
LONG_DOUBLE strtold(const char *nptr, char **endptr) {
    return (LONG_DOUBLE)strtod(nptr, endptr);
}
#else
float strtof(const char *nptr, char **endptr);
LONG_DOUBLE strtold(const char *nptr, char **endptr);
#endif

#define FTEST(prefix, typename, type, fmt)                                    \
    void prefix##cmp(type a, type b) {                                        \
        printf("%d %d %d %d %d %d\n",                                         \
               a == b,                                                        \
               a != b,                                                        \
               a<b,                                                           \
                 a>                                                           \
                   b,                                                         \
               a >= b,                                                        \
               a <= b);                                                       \
        printf(fmt " " fmt " " fmt " " fmt " " fmt " " fmt " " fmt "\n",      \
               a,                                                             \
               b,                                                             \
               a + b,                                                         \
               a - b,                                                         \
               a * b,                                                         \
               a / b,                                                         \
               -a);                                                           \
        printf(fmt "\n", ++a);                                                \
        printf(fmt "\n", a++);                                                \
        printf(fmt "\n", a);                                                  \
        b = 0;                                                                \
        printf("%d %d\n", !a, !b);                                            \
    }                                                                         \
    void prefix##fcast(type a) {                                              \
        float fa;                                                             \
        double da;                                                            \
        LONG_DOUBLE la;                                                       \
        int ia;                                                               \
        long long llia;                                                       \
        unsigned int ua;                                                      \
        unsigned long long llua;                                              \
        type b;                                                               \
        fa = a;                                                               \
        da = a;                                                               \
        la = a;                                                               \
        printf("ftof: %f %f %Lf\n", fa, da, la);                              \
        ia = (int)a;                                                          \
        llia = (long long)a;                                                  \
        a = (a >= 0) ? a : -a;                                                \
        ua = (unsigned int)a;                                                 \
        llua = (unsigned long long)a;                                         \
        printf("ftoi: %d %u %lld %llu\n", ia, ua, llia, llua);                \
        ia = -1234;                                                           \
        ua = 0x81234500;                                                      \
        llia = -0x123456789012345LL;                                          \
        llua = 0xf123456789012345LLU;                                         \
        b = ia;                                                               \
        printf("itof: " fmt "\n", b);                                         \
        b = ua;                                                               \
        printf("utof: " fmt "\n", b);                                         \
        b = llia;                                                             \
        printf("lltof: " fmt "\n", b);                                        \
        b = llua;                                                             \
        if (CC_NAME != CC_clang)                                              \
            printf("ulltof: " fmt "\n", b);                                   \
    }                                                                         \
                                                                              \
    float prefix##retf(type a) {                                              \
        return a;                                                             \
    }                                                                         \
    double prefix##retd(type a) {                                             \
        return a;                                                             \
    }                                                                         \
    LONG_DOUBLE prefix##retld(type a) {                                       \
        return a;                                                             \
    }                                                                         \
                                                                              \
    void prefix##call(void) {                                                 \
        printf("float: " FLOAT_FMT, prefix##retf(42.123456789));              \
        printf("double: %f\n", prefix##retd(42.123456789));                   \
        printf("long double: %Lf\n", prefix##retld(42.123456789));            \
        printf("strto%s: %f\n", #prefix, (double)strto##prefix("1.2", NULL)); \
    }                                                                         \
                                                                              \
    void prefix##signed_zeros(void) {                                         \
        type x = 0.0, y = -0.0, n, p;                                         \
        if (x == y)                                                           \
            printf("Test 1.0 / x != 1.0 / y  returns %d (should be 1).\n",    \
                   1.0 / x != 1.0 / y);                                       \
        else                                                                  \
            printf("x != y; this is wrong!\n");                               \
                                                                              \
        n = -x;                                                               \
        if (x == n)                                                           \
            printf("Test 1.0 / x != 1.0 / -x returns %d (should be 1).\n",    \
                   1.0 / x != 1.0 / n);                                       \
        else                                                                  \
            printf("x != -x; this is wrong!\n");                              \
                                                                              \
        p = +y;                                                               \
        if (x == p)                                                           \
            printf("Test 1.0 / x != 1.0 / +y returns %d (should be 1).\n",    \
                   1.0 / x != 1.0 / p);                                       \
        else                                                                  \
            printf("x != +y; this is wrong!\n");                              \
        p = -y;                                                               \
        if (x == p)                                                           \
            printf("Test 1.0 / x != 1.0 / -y returns %d (should be 0).\n",    \
                   1.0 / x != 1.0 / p);                                       \
        else                                                                  \
            printf("x != -y; this is wrong!\n");                              \
    }                                                                         \
    void prefix##nan(void) {                                                  \
        type nan = 0.0 / 0.0;                                                 \
        type nnan = -nan;                                                     \
        printf("nantest: " fmt " " fmt "\n", nan, nnan);                      \
    }                                                                         \
    void prefix##test(void) {                                                 \
        printf("testing '%s'\n", #typename);                                  \
        prefix##cmp(1, 2.5);                                                  \
        prefix##cmp(2, 1.5);                                                  \
        prefix##cmp(1, 1);                                                    \
        prefix##fcast(234.6);                                                 \
        prefix##fcast(-2334.6);                                               \
        prefix##call();                                                       \
        prefix##signed_zeros();                                               \
        if (CC_NAME != CC_clang)                                              \
            prefix##nan();                                                    \
    }

FTEST(f, float, float, "%f")
FTEST(d, double, double, "%f")
FTEST(ld, long double, LONG_DOUBLE, "%Lf")

double ftab1[3] = {1.2, 3.4, -5.6};
