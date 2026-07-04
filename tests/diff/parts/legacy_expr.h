void if_test(void) {
    if1t(1, 0, 0, 0);
    if1t(2, 0, 3, 0);
    if1t(3, 2, 0, 0);
    if1t(4, 2, 3, 0);
    if2t();
    if3t();
}

void loop_test() {
    int i;
    i = 0;
    while (i < 10)
        printf("%d", i++);
    printf("\n");
    for (i = 0; i < 10; i++)
        printf("%d", i);
    printf("\n");
    i = 0;
    do {
        printf("%d", i++);
    } while (i < 10);
    printf("\n");

    char count = 123;

    for (size_t count = 1; count < 3; count++)
        printf("count=%d\n", count);
    printf("count = %d\n", count);

    i = 0;
    while (1) {
        if (i == 6)
            break;
        i++;
        if (i == 3)
            continue;
        printf("%d", i);
    }
    printf("\n");

    i = 0;
    do {
        if (i == 6)
            break;
        i++;
        if (i == 3)
            continue;
        printf("%d", i);
    } while (1);
    printf("\n");

    for (i = 0; i < 10; i++) {
        if (i == 3)
            continue;
        printf("%d", i);
    }
    printf("\n");
}

typedef int typedef_and_label;

void goto_test() {
    int i;
    static void *label_table[3] = {&&label1, &&label2, &&label3};
    struct {
        int bla;

        typedef_and_label : 32;
    } y = {1};

    printf("\ngoto:\n");
    i = 0;

    typedef_and_label x;

typedef_and_label:
s_loop:
    if (i >= 10)
        goto s_end;
    printf("%d", i);
    i++;
    goto s_loop;
s_end:
    printf("\n");

    for (i = 0; i < 3; i++) {
        goto *label_table[i];
    label1:
        printf("label1\n");
        goto next;
    label2:
        printf("label2\n");
        goto next;
    label3:
        printf("label3\n");
    next:;
    }
}

enum {
    E0,
    E1 = 2,
    E2 = 4,
    E3,
    E4,
};

enum test {
    E5 = 1000,
};

struct S_enum {
    enum { E6 = 42,
           E7,
           E8 } e : 8;
};

enum ELong {

    EL_large = ((unsigned long)0xf000 << 31) << 1,
};

enum { BIASU = -1U << 31 };
enum { BIASS = -1 << 31 };

static int getint(int i) {
    if (i)
        return 0;
    else
        return (int)(-1U << 31);
}

void enum_test() {
    enum test b1;

    unsigned *p = &b1;
    struct S_enum s = {E7};
    printf("%d %d %d %d %d %d %d\n", s.e,
           E0, E1, E2, E3, E4, E5);
    b1 = 1;
    printf("b1=%d\n", b1);
    printf("enum large: %ld\n", EL_large);

    if (getint(0) == BIASU)
        printf("enum unsigned: ok\n");
    else
        printf("enum unsigned: wrong\n");
    if (getint(0) == BIASS)
        printf("enum unsigned: ok\n");
    else
        printf("enum unsigned: wrong\n");
}

typedef int *my_ptr;

typedef int mytype1;
typedef int mytype2;

void typedef_test() {
    my_ptr a;
    mytype1 mytype2;
    int b;

    a = &b;
    *a = 1234;
    printf("a=%d\n", *a);
    mytype2 = 2;
    printf("mytype2=%d\n", mytype2);
}

void forward_test() {
    forward_ref();
    forward_ref();
}

void forward_ref(void) {
    printf("forward ok\n");
}

typedef struct struct1 {
    int f1;
    int f2, f3;
    union union1 {
        int v1;
        int v2;
    } u;
    char str[3];
} struct1;

struct struct2 {
    int a;
    char b;
};

union union2 {
    int w1;
    int w2;
};

struct struct1 st1, st2;

struct empty_mem {
    ;
    int x;
};

int tab[3];
int tab2[3][2];

int g;

void f1(int g) {
    printf("g1=%d\n", g);
}

void scope_test() {
    g = 2;
    f1(1);
    printf("g2=%d\n", g);
    {
        int g;
        g = 3;
        printf("g3=%d\n", g);
        {
            int g;
            g = 4;
            printf("g4=%d\n", g);
        }
    }
    printf("g5=%d\n", g);
}

int st2_i;
int *st2_p = &st2_i;
void scope2_test() {
    char a[50];
    st2_i = 42;
    for (int st2_i = 1; st2_i < 10; st2_i++) {
        extern int st2_i;
        st2_i++;
        printf("exloc: %d\n", st2_i);
    }
    printf("exloc: %d\n", *st2_p);
}

extern int st_global1;
int st_global1 = 42;
extern int st_global1;
int st_global1;
extern int st_global2;
int st_global2;
extern int st_global2;
int st_global2;

void array_test() {
    int i, j, a[4];

    printf("sizeof(a) = %d\n", sizeof(a));
    printf("sizeof(\"a\") = %d\n", sizeof("a"));
#ifdef C99_MACROS
    printf("sizeof(__func__) = %d\n", sizeof(__func__));
#endif
    printf("sizeof tab %d\n", sizeof(tab));
    printf("sizeof tab2 %d\n", sizeof tab2);
    tab[0] = 1;
    tab[1] = 2;
    tab[2] = 3;
    printf("%d %d %d\n", tab[0], tab[1], tab[2]);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 2; j++)
            tab2[i][j] = 10 * i + j;
    for (i = 0; i < 3 * 2; i++) {
        printf(" %3d", ((int *)tab2)[i]);
    }
    printf("\n");
    printf("sizeof(size_t)=%d\n", sizeof(size_t));
    printf("sizeof(ptrdiff_t)=%d\n", sizeof(ptrdiff_t));
}

void expr_test() {
    int a, b;
    a = 0;
    printf("%d\n", a += 1);
    printf("%d\n", a -= 2);
    printf("%d\n", a *= 31232132);
    printf("%d\n", a /= 4);
    printf("%d\n", a %= 20);
    printf("%d\n", a &= 6);
    printf("%d\n", a ^= 7);
    printf("%d\n", a |= 8);
    printf("%d\n", a >>= 3);
    printf("%d\n", a <<= 4);

    a = 22321;
    b = -22321;
    printf("%d\n", a + 1);
    printf("%d\n", a - 2);
    printf("%d\n", a * 312);
    printf("%d\n", a / 4);
    printf("%d\n", b / 4);
    printf("%d\n", (unsigned)b / 4);
    printf("%d\n", a % 20);
    printf("%d\n", b % 20);
    printf("%d\n", (unsigned)b % 20);
    printf("%d\n", a & 6);
    printf("%d\n", a ^ 7);
    printf("%d\n", a | 8);
    printf("%d\n", a >> 3);
    printf("%d\n", b >> 3);
    printf("%d\n", (unsigned)b >> 3);
    printf("%d\n", a << 4);
    printf("%d\n", ~a);
    printf("%d\n", -a);
    printf("%d\n", +a);

    printf("%d\n", 12 + 1);
    printf("%d\n", 12 - 2);
    printf("%d\n", 12 * 312);
    printf("%d\n", 12 / 4);
    printf("%d\n", 12 % 20);
    printf("%d\n", 12 & 6);
    printf("%d\n", 12 ^ 7);
    printf("%d\n", 12 | 8);
    printf("%d\n", 12 >> 2);
    printf("%d\n", 12 << 4);
    printf("%d\n", ~12);
    printf("%d\n", -12);
    printf("%d\n", +12);
    printf("%d %d %d %d\n",
           isid('a'),
           isid('g'),
           isid('T'),
           isid('('));
}

int isid(int c) {
    return (c >= 'a' & c <= 'z') | (c >= 'A' & c <= 'Z') | c == '_';
}

int vstack[10], *vstack_ptr;

void vpush(int vt, int vc) {
    *vstack_ptr++ = vt;
    *vstack_ptr++ = vc;
}

void vpop(int *ft, int *fc) {
    *fc = *--vstack_ptr;
    *ft = *--vstack_ptr;
}

void expr2_test() {
    int a, b;

    vstack_ptr = vstack;
    vpush(1432432, 2);
    vstack_ptr[-2] &= ~0xffffff80;
    vpop(&a, &b);
    printf("res= %d %d\n", a, b);
}

int const_len_ar[sizeof(1 / 0)];

void constant_expr_test() {
    int a;
    a = 3;
    printf("%d\n", a * 16);
    printf("%d\n", a * 1);
    printf("%d\n", a + 0);
    printf("%d\n", sizeof(const_len_ar));
}

int tab4[10];

void expr_ptr_test() {
    int arr[10], *p, *q;
    int i = -1;

    p = tab4;
    q = tab4 + 10;
    printf("diff=%d\n", q - p);
    p++;
    printf("inc=%d\n", p - tab4);
    p--;
    printf("dec=%d\n", p - tab4);
    ++p;
    printf("inc=%d\n", p - tab4);
    --p;
    printf("dec=%d\n", p - tab4);
    printf("add=%d\n", p + 3 - tab4);
    printf("add=%d\n", 3 + p - tab4);

    q = p = 0;
    q += i;
    printf("%p %p %ld\n", q, p, p - q);
    printf("%d %d %d %d %d %d\n",
           p == q, p != q, p<q, p <= q, p >= q, p> q);
    i = 0xf0000000;
    p += i;
    printf("%p %p %ld\n", q, p, p - q);
    printf("%d %d %d %d %d %d\n",
           p == q, p != q, p<q, p <= q, p >= q, p> q);
    p = (int *)((char *)p + 0xf0000000);
    printf("%p %p %ld\n", q, p, p - q);
    printf("%d %d %d %d %d %d\n",
           p == q, p != q, p<q, p <= q, p >= q, p> q);
    p += 0xf0000000;
    printf("%p %p %ld\n", q, p, p - q);
    printf("%d %d %d %d %d %d\n",
           p == q, p != q, p<q, p <= q, p >= q, p> q);
    {
        struct size12 {
            int i, j, k;
        };
        struct size12 s[2], *sp = s;
        int i, j;
        sp->i = 42;
        sp++;
        j = -1;
        printf("%d\n", sp[j].i);
    }
#ifdef __LP64__
    i = 1;
    p = (int *)0x100000000UL + i;
    i = ((long)p) >> 32;
    printf("largeptr: %p %d\n", p, i);
#endif
    p = &arr[0];
    q = p + 3;
    printf("%d\n", (int)((p - q) / 3));
}

void expr_cmp_test() {
    int a, b;
    a = -1;
    b = 1;
    printf("%d\n", a == a);
    printf("%d\n", a != a);

    printf("%d\n", a < b);
    printf("%d\n", a <= b);
    printf("%d\n", a <= a);
    printf("%d\n", b >= a);
    printf("%d\n", a >= a);
    printf("%d\n", b > a);

    printf("%d\n", (unsigned)a < b);
    printf("%d\n", (unsigned)a <= b);
    printf("%d\n", (unsigned)a <= a);
    printf("%d\n", (unsigned)b >= a);
    printf("%d\n", (unsigned)a >= a);
    printf("%d\n", (unsigned)b > a);
}

struct empty {
};

struct aligntest1 {
    char a[10];
};

struct aligntest2 {
    int a;
    char b[10];
};

struct aligntest3 {
    double a, b;
};

struct aligntest4 {
    double a[0];
};

struct __attribute__((aligned(16))) aligntest5 {
    int i;
};
struct aligntest6 {
    int i;
} __attribute__((aligned(16)));
struct aligntest7 {
    int i;
};
struct aligntest5 altest5[2];
struct aligntest6 altest6[2];
int pad1;

struct aligntest7 altest7[2] __attribute__((aligned(16)));

struct aligntest8 {
    int i;
} __attribute__((aligned(4096)));

struct Large {
    unsigned long flags;
    union {
        void *u1;
        int *u2;
    };

    struct {
        union {
            unsigned long index;
            void *freelist;
        };
        union {
            unsigned long counters;
            struct {
                int bla;
            };
        };
    };

    union {
        struct {
            long u3;
            long u4;
        };
        void *u5;
        struct {
            unsigned long compound_head;
            unsigned int compound_dtor;
            unsigned int compound_order;
        };
    };
} __attribute__((aligned(2 * sizeof(long))));

typedef unsigned long long __attribute__((aligned(4))) unaligned_u64;

struct aligntest9 {
    unsigned int buf_nr;
    unaligned_u64 start_lba;
};

struct aligntest10 {
    unsigned int buf_nr;
    unsigned long long start_lba;
};

void struct_test() {
    struct1 *s;
    union union2 u;
    struct Large ls;

    printf("sizes: %d %d %d %d\n",
           sizeof(struct struct1),
           sizeof(struct struct2),
           sizeof(union union1),
           sizeof(union union2));
    printf("offsets: %d\n", (int)((char *)&st1.u.v1 - (char *)&st1));
    st1.f1 = 1;
    st1.f2 = 2;
    st1.f3 = 3;
    printf("st1: %d %d %d\n",
           st1.f1, st1.f2, st1.f3);
    st1.u.v1 = 1;
    st1.u.v2 = 2;
    printf("union1: %d\n", st1.u.v1);
    u.w1 = 1;
    u.w2 = 2;
    printf("union2: %d\n", u.w1);
    s = &st2;
    s->f1 = 3;
    s->f2 = 2;
    s->f3 = 1;
    printf("st2: %d %d %d\n",
           s->f1, s->f2, s->f3);
    printf("str_addr=%x\n", (int)(uintptr_t)st1.str - (int)(uintptr_t)&st1.f1);

    printf("aligntest1 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest1), __alignof__(struct aligntest1));
    printf("aligntest2 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest2), __alignof__(struct aligntest2));
    printf("aligntest3 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest3), __alignof__(struct aligntest3));
#if !(defined _WIN32 && CC_NAME == CC_clang)
    printf("aligntest4 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest4), __alignof__(struct aligntest4));
#endif
    printf("aligntest5 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest5), __alignof__(struct aligntest5));
    printf("aligntest6 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest6), __alignof__(struct aligntest6));
    printf("aligntest7 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest7), __alignof__(struct aligntest7));
    printf("aligntest8 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest8), __alignof__(struct aligntest8));
#if !(defined _WIN32 && CC_NAME == CC_clang)
    printf("aligntest9 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest9), __alignof__(struct aligntest9));
#endif
    printf("aligntest10 sizeof=%d alignof=%d\n",
           sizeof(struct aligntest10), __alignof__(struct aligntest10));
    printf("altest5 sizeof=%d alignof=%d\n",
           sizeof(altest5), __alignof__(altest5));
    printf("altest6 sizeof=%d alignof=%d\n",
           sizeof(altest6), __alignof__(altest6));
    printf("altest7 sizeof=%d alignof=%d\n",
           sizeof(altest7), __alignof__(altest7));

#if !(defined _WIN32 && CC_NAME == CC_clang)
    printf("sizeof(struct empty) = %d\n", sizeof(struct empty));
#endif
    printf("alignof(struct empty) = %d\n", __alignof__(struct empty));

    printf("Large: sizeof=%d\n", sizeof(ls));
    memset(&ls, 0, sizeof(ls));
    ls.compound_head = 42;
    printf("Large: offsetof(compound_head)=%d\n", (int)((char *)&ls.compound_head - (char *)&ls));
}

static int __csf(int x) {
    return x;
}
static void *_csf = __csf;
#define csf(t, n) ((t (*)(int))_csf)(n)

void char_short_test() {
    int var1, var2;
    signed char var3;
    long long var4;

    var1 = 0x01020304;
    var2 = 0xfffefdfc;
    printf("s8=%d %d\n",
           *(signed char *)&var1, *(signed char *)&var2);
    printf("u8=%d %d\n",
           *(unsigned char *)&var1, *(unsigned char *)&var2);
    printf("s16=%d %d\n",
           *(short *)&var1, *(short *)&var2);
    printf("u16=%d %d\n",
           *(unsigned short *)&var1, *(unsigned short *)&var2);
    printf("s32=%d %d\n",
           *(int *)&var1, *(int *)&var2);
    printf("u32=%d %d\n",
           *(unsigned int *)&var1, *(unsigned int *)&var2);
    *(signed char *)&var1 = 0x08;
    printf("var1=%x\n", var1);
    *(short *)&var1 = 0x0809;
    printf("var1=%x\n", var1);
    *(int *)&var1 = 0x08090a0b;
    printf("var1=%x\n", var1);

    var1 = 0x778899aa;
    var4 = 0x11223344aa998877ULL;
    var1 = var3 = var1 + 1;
    var4 = var3 = var4 + 1;
    printf("promote char/short assign %d " LONG_LONG_FORMAT "\n", var1, var4);
    var1 = 0x778899aa;
    var4 = 0x11223344aa998877ULL;
    printf("promote char/short assign VA %d %d\n", var3 = var1 + 1, var3 = var4 + 1);
    printf("promote char/short cast VA %d %d\n", (signed char)(var1 + 1), (signed char)(var4 + 1));
#if !defined __arm__ && !defined __riscv

    var1 = csf(unsigned char, 0x89898989);
    var4 = csf(signed char, 0xabababab);
#ifdef __clang__

    var1 &= 0xff;
#endif
    printf("promote char/short funcret %d " LONG_LONG_FORMAT "\n", var1, var4);
    printf("promote char/short fumcret VA %d %d %d %d\n",
           csf(unsigned short, 0xcdcdcdcd),
           csf(short, 0xefefefef),
           csf(_Bool, 0x33221100),
           csf(_Bool, 0x33221101));
#endif
    var3 = -10;
    var1 = (signed char)(unsigned char)(var3 + 1);
    var4 = (signed char)(unsigned char)(var3 + 1);
    printf("promote multicast (char)(unsigned char) %d " LONG_LONG_FORMAT "\n", var1, var4);
    var4 = 0x11223344aa998877ULL;
    var4 = (unsigned)(int)(var4 + 1);
    printf("promote multicast (unsigned)(int) " LONG_LONG_FORMAT "\n", var4);
    var4 = 0x11223344bbaa9988ULL;
    var4 = (unsigned)(signed char)(var4 + 1);
    printf("promote multicast (unsigned)(char) " LONG_LONG_FORMAT "\n", var4);
}

typedef struct Sym {
    int v;
    int t;
    int c;
    struct Sym *next;
    struct Sym *prev;
} Sym;

#define ISLOWER(c) ('a' <= (c) && (c) <= 'z')
#define TOUPPER(c) (ISLOWER(c) ? 'A' + ((c) - 'a') : (c))

static int toupper1(int a) {
    return TOUPPER(a);
}

static unsigned int calc_vm_flags(unsigned int prot) {
    unsigned int prot_bits;

    prot_bits = ((0x1 == 0x00000001) ? (prot & 0x1) : (prot & 0x1) ? 0x00000001
                                                                   : 0);
    return prot_bits;
}

enum cast_enum { FIRST,
                 LAST };

static void tst_cast(enum cast_enum ce) {
    printf("%d\n", ce);
}

void bool_test() {
    int *s, a, b, t, f, i;

    a = 0;
    s = (void *)0;
    printf("!s=%d\n", !s);

    if (!s || !s[0])
        a = 1;
    printf("a=%d\n", a);

    printf("a=%d %d %d\n", 0 || 0, 0 || 1, 1 || 1);
    printf("a=%d %d %d\n", 0 && 0, 0 && 1, 1 && 1);
    printf("a=%d %d\n", 1 ? 1 : 0, 0 ? 1 : 0);
#if 1 && 1
    printf("a1\n");
#endif
#if 1 || 0
    printf("a2\n");
#endif
#if 1 ? 0 : 1
    printf("a3\n");
#endif
#if 0 ? 0 : 1
    printf("a4\n");
#endif

    a = 4;
    printf("b=%d\n", a + (0 ? 1 : a / 2));

    a = 10;
    b = 10;
    a = (a + b) * ((a < b) ? ((b - a) * (a - b)) : a + b);
    printf("a=%d\n", a);

    t = 1;
    f = 0;
    a = 32;
    printf("exp=%d\n", f == (32 <= a && a <= 3));
    printf("r=%d\n", (t || f) + (t && f));

    t = 1;
    printf("type of bool: %d\n", (int)((~((unsigned int)(t && 1))) / 2));
    tst_cast(t >= 0 ? FIRST : LAST);

    printf("type of cond: %d\n", (~(t ? 0U : (unsigned int)0)) / 2);

    {
        int aspect_on;
        int aspect_native = 65536;
        double bfu_aspect = 1.0;
        int aspect;
        for (aspect_on = 0; aspect_on < 2; aspect_on++) {
            aspect = aspect_on ? (aspect_native * bfu_aspect + 0.5) : 65535UL;
            printf("aspect=%d\n", aspect);
        }
    }

    {
        static int v1 = 34 ?: -1;
        static int v2 = 0 ?: -1;
        int a = 30;

        printf("%d %d\n", v1, v2);
        printf("%d %d\n", a - 30 ?: a * 2, a + 1 ?: a * 2);
    }

    for (i = 0; i < 256; i++) {
        if (toupper1(i) != TOUPPER(i))
            printf("error %d\n", i);
    }
    printf("bits = 0x%x\n", calc_vm_flags(0x1));
}

extern int undefined_function(void);
extern int defined_function(void);

#ifdef __clang__
int undefined_function(void) {
}
#endif

static inline void refer_to_undefined(void) {
    undefined_function();
}
