void sizeof_test(void) {
    int a;
    int **ptr;

    printf("sizeof(int) = %d\n", sizeof(int));
    printf("sizeof(unsigned int) = %d\n", sizeof(unsigned int));
    printf("sizeof(long) = %d\n", sizeof(long));
    printf("sizeof(unsigned long) = %d\n", sizeof(unsigned long));
    printf("sizeof(short) = %d\n", sizeof(short));
    printf("sizeof(unsigned short) = %d\n", sizeof(unsigned short));
    printf("sizeof(char) = %d\n", sizeof(char));
    printf("sizeof(unsigned char) = %d\n", sizeof(unsigned char));
    printf("sizeof(func) = %d\n", sizeof sizeof_test());
    a = 1;
    printf("sizeof(a++) = %d\n", sizeof a++);
    printf("a=%d\n", a);
    ptr = NULL;
    printf("sizeof(**ptr) = %d\n", sizeof(**ptr));

    printf("sizeof(sizeof(int) = %d\n", sizeof(sizeof(int)));
    uintptr_t t = 1;
    uintptr_t t2;

    t <<= 16;
    t <<= 16;
    t++;

    t2 = t & -sizeof(uintptr_t);
    printf("%lu %lu\n", t, t2);

    printf("__alignof__(int) = %d\n", __alignof__(int));
    printf("__alignof__(unsigned int) = %d\n", __alignof__(unsigned int));
    printf("__alignof__(short) = %d\n", __alignof__(short));
    printf("__alignof__(unsigned short) = %d\n", __alignof__(unsigned short));
    printf("__alignof__(char) = %d\n", __alignof__(char));
    printf("__alignof__(unsigned char) = %d\n", __alignof__(unsigned char));
    printf("__alignof__(func) = %d\n", __alignof__ sizeof_test());

    a = 2;
    printf("sizeof(char[1+2*a]) = %d\n", sizeof(char[1 + 2 * a]));

    printf("sizeof( (struct {int i; int j;}){4,5} ) = %d\n",
           sizeof((struct {int i; int j; }){4, 5}));

    printf("sizeof (struct {short i; short j;}){4,5} = %d\n",
           sizeof(struct {short i; short j; }){4, 5});

    printf("sizeof(t && 0) = %d\n", sizeof(t && 0));
    printf("sizeof(1 && 1) = %d\n", sizeof(1 && 1));
    printf("sizeof(t || 1) = %d\n", sizeof(t || 1));
    printf("sizeof(0 || 0) = %d\n", sizeof(0 || 0));

    int arr[4], fn();
    printf("sizeof(0, arr) = %d\n", sizeof(0, arr));
    printf("sizeof(0, fn) = %d\n", sizeof(0, fn));
}

void typeof_test(void) {
    double a;
    typeof(a) b;
    typeof(float) c;

    a = 1.5;
    b = 2.5;
    c = 3.5;
    printf("a=%f b=%f c=%f\n", a, b, c);
}

struct hlist_node;
struct hlist_head {
    struct hlist_node *first, *last;
};

void consume_ulong(unsigned long i) {
    i = 0;
}

void statement_expr_test(void) {
    int a, i;

    a = 0;
    for (i = 0; i < 10; i++) {
        a += 1 +
             ({
                 int b, j;
                 b = 0;
                 for (j = 0; j < 5; j++)
                     b += j;
                 b;
             });
    }
    printf("a=%d\n", a);

    void *v = (void *)39;
    typeof(({
        (struct hlist_node *)v;
    })) x;
    typeof(x)
        ptr = (struct hlist_node *)v;

#define some_attr __attribute__((aligned(1)))
#define tps(str) ({                       \
    static const char *t some_attr = str; \
    t;                                    \
})
    printf("stmtexpr: %s %s\n",
           tps("somerandomlongstring"),
           tps("anotherlongstring"));

    int t = 40;
    int b = ({ int t = 41; t; });
    int c = ({ int t = 42; t; });

    struct hlist_head h = ({
        typedef struct hlist_head T;
        long pre = 48;
        T t = {(void *)43, (void *)44};
        long post = 49;
        t;
    });
    printf("stmtexpr: %d %d %d\n", t, b, c);
    printf("stmtexpr: %ld %ld\n", (long)h.first, (long)h.last);

    consume_ulong(({ __label__ __here; __here: (unsigned long)&&__here; }));

    i = 0;
    ({
        {
            __label__ LBL;
        LBL:
            if (i++ == 0)
                goto LBL;
        }

        goto LBL;
    });
LBL:
    printf("stmtexpr: %d should be 2\n", i);
}

void local_label_test(void) {
    int a;
    goto l1;
l2:
    a = 1 + ({
            __label__ l1, l2, l3, l4;
            goto l1;
        l4:
            printf("aa1\n");
            goto l3;
        l2:
            printf("aa3\n");
            goto l4;
        l1:
            printf("aa2\n");
            goto l2;
        l3:;
            1;
        });
    printf("a=%d\n", a);
    return;
l4:
    printf("bb1\n");
    goto l2;
l1:
    printf("bb2\n");
    goto l4;
}

#if (defined(__i386__) || defined(__x86_64__)) && !(defined _WIN32 && CC_NAME == CC_clang)

typedef __SIZE_TYPE__ word;

static char *strncat1(char *dest, const char *src, size_t count) {
    word d0, d1, d2, d3;
    __asm__ __volatile__(
        "repne\n\t"
        "scasb\n\t"
        "dec %1\n\t"
        "mov %8,%3\n"
        "1:\tdec %3\n\t"
        "js 2f\n\t"
        "lodsb\n\t"
        "stosb\n\t"
        "testb %%al,%%al\n\t"
        "jne 1b\n"
        "2:\txor %2,%2\n\t"
        "stosb"
        : "=&S"(d0), "=&D"(d1), "=&a"(d2), "=&c"(d3)
        : "0"(src), "1"(dest), "2"(0), "3"(0xffffffff), "g"(count)
        : "memory");
    return dest;
}

static char *strncat2(char *dest, const char *src, size_t count) {
    word d0, d1, d2, d3;
    __asm__ __volatile__(
        "repne scasb\n\t"
        "dec %1\n\t"
        "mov %8,%3\n"
        "1:\tdec %3\n\t"
        "js 2f\n\t"
        "lodsb\n\t"
        "stosb\n\t"
        "testb %%al,%%al\n\t"
        "jne 1b\n"
        "2:\txor %2,%2\n\t"
        "stosb"
        : "=&S"(d0), "=&D"(d1), "=&a"(d2), "=&c"(d3)
        : "0"(src), "1"(dest), "2"(0), "3"(0xffffffff), "g"(count)
        : "memory");
    return dest;
}

static inline void *memcpy1(void *to, const void *from, size_t n) {
    word d0, d1, d2;
    __asm__ __volatile__(
        "rep ; movsl\n\t"
        "testb $2,%b4\n\t"
        "je 1f\n\t"
        "movsw\n"
        "1:\ttestb $1,%b4\n\t"
        "je 2f\n\t"
        "movsb\n"
        "2:"
        : "=&c"(d0), "=&D"(d1), "=&S"(d2)
        : "0"(n / 4), "q"(n), "1"((word)to), "2"((word)from)
        : "memory");
    return (to);
}

static inline void *memcpy2(void *to, const void *from, size_t n) {
    word d0, d1, d2;
    __asm__ __volatile__(
        "rep movsl\n\t"
        "testb $2,%b4\n\t"
        "je 1f\n\t"
        "movsw\n"
        "1:\ttestb $1,%b4\n\t"
        "je 2f\n\t"
        "movsb\n"
        "2:"
        : "=&c"(d0), "=&D"(d1), "=&S"(d2)
        : "0"(n / 4), "q"(n), "1"((word)to), "2"((word)from)
        : "memory");
    return (to);
}

static __inline__ void sigaddset1(unsigned int *set, int _sig) {
    __asm__("btsl %1,%0" : "=m"(*set) : "Ir"(_sig - 1) : "cc");
}

static __inline__ void sigdelset1(unsigned int *set, int _sig) {
    asm("btrl %1,%0" : "=m"(*set) : "Ir"(_sig - 1) : "cc", "flags");
}

#ifdef __clang__

static __inline__ __const__ unsigned int swab32(unsigned int x) {
    return ((x >> 24) & 0xff) |
           ((x >> 8) & 0xff00) |
           ((x << 8) & 0xff0000) |
           ((x << 24) & 0xff000000);
}
#else
static __inline__ __const__ unsigned int swab32(unsigned int x) {
    __asm__("xchgb %b0,%h0\n\t"
            "rorl $16,%0\n\t"
            "xchgb %b0,%h0"
            : "="
              "q"(x)
            : "0"(x));
    return x;
}
#endif

static __inline__ unsigned long long mul64(unsigned int a, unsigned int b) {
    unsigned long long res;
#ifdef __x86_64__

    unsigned int resh, resl;
    __asm__("mull %2" : "=a"(resl), "=d"(resh) : "a"(a), "r"(b));
    res = ((unsigned long long)resh << 32) | resl;
#else
    __asm__("mull %2" : "=A"(res) : "a"(a), "r"(b));
#endif
    return res;
}

static __inline__ unsigned long long inc64(unsigned long long a) {
    unsigned long long res;
#ifdef __x86_64__

    res = a + 1;
#else
    __asm__("addl $1, %%eax ; adcl $0, %%edx" : "=A"(res) : "A"(a));
#endif
    return res;
}

struct struct123 {
    int a;
    int b;
};
struct struct1231 {
    word addr;
};

word mconstraint_test(struct struct1231 *r) {
    word ret;
    unsigned int a[2];
    a[0] = 0;
    __asm__ volatile("lea %2,%0; movl 4(%0),%k0; addl %2,%k0; movl $51,%2; movl $52,4%2; movl $63,%1"
                     : "=&r"(ret), "=m"(a)
                     : "m"(*(struct struct123 *)r->addr));
    return ret + a[0];
}

#ifdef __x86_64__
int fls64(unsigned long long x) {
    int bitpos = -1;
    asm("bsrq %1,%q0"
        : "+r"(bitpos)
        : "rm"(x));
    return bitpos + 1;
}
#endif

void other_constraints_test(void) {
    word ret;
    int var;
#if CC_NAME != CC_clang
    __asm__ volatile("mov %P1,%0" : "=r"(ret) : "p"(&var));
    printf("oc1: %d\n", ret == (word)&var);
#endif
}

#ifndef _WIN32

void base_func(void) {
    printf("asmc: base\n");
}

#ifndef __APPLE__
extern void override_func1(void);
extern void override_func2(void);

asm(".weak override_func1\n.set override_func1, base_func");
asm(".set override_func1, base_func");
asm(".set override_func2, base_func");

void override_func2(void) {
    printf("asmc: override2\n");
}

extern int bug_table[] __attribute__((section("__bug_table")));
char *get_asm_string(void) {

#ifndef __clang__
    extern int some_symbol;
    asm volatile(".globl some_symbol\n"
                 "jmp .+6\n"
                 "1:\n"
                 "some_symbol: .long 0\n"
                 ".pushsection __bug_table, \"a\"\n"
                 ".globl bug_table\n"
                 "bug_table:\n"

                 "2:\t.long 1b - 2b, %c0 - 2b\n"
                 ".popsection\n" : : "i"("A string"));
    char *str = ((char *)bug_table) + bug_table[1];
    return str;
#else
    return (char *)"A string";
#endif
}

extern unsigned char alld_stuff[];
asm(".data\n"
    ".byte 41\n"
    "alld_stuff:\n"
    "661:\n"
    ".byte 42\n"
    "662:\n"
    ".pushsection .data.ignore\n"
    ".long 661b - .\n"

    ".popsection\n"
    ".byte 662b - 661b\n");

void asm_local_label_diff(void) {
    printf("asm_local_label_diff: %d %d\n", alld_stuff[0], alld_stuff[1]);
}
#endif
#endif

void asm_local_statics(void) {
    static int localint = 41;
    asm("incl %0" : "+m"(localint));
    printf("asm_local_statics: %d\n", localint);
}

static unsigned int set;

void fancy_copy(unsigned *in, unsigned *out) {
    asm volatile("" : "=r"(*out) : "0"(*in));
}

void fancy_copy2(unsigned *in, unsigned *out) {
    asm volatile("mov %0,(%1)" : : "r"(*in), "r"(out) : "memory");
}

#if defined __x86_64__
void clobber_r12(void) {
    asm volatile("mov $1, %%r12" ::: "r12");
}
#endif

void test_high_clobbers_really(void) {
#if defined __x86_64__
    register word val asm("r12");
    word val2;

    asm volatile("mov $0x4542, %%r12" : "=r"(val)::"memory");
    clobber_r12();
    asm volatile("mov %%r12, %0" : "=r"(val2) : "r"(val) : "memory");
    printf("asmhc: 0x%x\n", val2);
#endif
}

void test_high_clobbers(void) {
#if defined __x86_64__
    word x1, x2;
    asm volatile("mov %%r12,%0" ::"m"(x1));
    test_high_clobbers_really();
    asm volatile("mov %%r12,%0" ::"m"(x2));
    asm volatile("mov %0,%%r12" ::"m"(x1));

#endif
}

static long cpu_number;
void trace_console(long len, long len2) {
#ifdef __x86_64__

    if (0 &&
        ({
            long pscr_ret__;
            switch (len) {
            case 4: {
                long pfo_ret__;
                switch (len2) {
                case 8:
                    printf("bla");
                    pfo_ret__ = 42;
                    break;
                }
                pscr_ret__ = pfo_ret__;
            } break;
            case 8: {
                long pfo_ret__;
                switch (len2) {
                case 1:
                    asm("movq %1,%0" : "=r"(pfo_ret__) : "m"(cpu_number));
                    break;
                case 2:
                    asm("movq %1,%0" : "=r"(pfo_ret__) : "m"(cpu_number));
                    break;
                case 4:
                    asm("movq %1,%0" : "=r"(pfo_ret__) : "m"(cpu_number));
                    break;
                case 8:
                    asm("movq %1,%0" : "=r"(pfo_ret__) : "m"(cpu_number));
                    break;
                default:
                    printf("impossible\n");
                }
                pscr_ret__ = pfo_ret__;
            };
                break;
            }
            pscr_ret__;
        })) {
        printf("huh?\n");
    }
#endif
}

void test_asm_dead_code(void) {
    word rdi;

    asm volatile("" : "=D"(rdi) : "0"(0));
    (void)sizeof(({
        int var;

        asm volatile("movl $0,(%0)" : : "D"(&var) : "memory");
        var;
    }));
}

void test_asm_call(void) {
#if defined __x86_64__ && !defined _WIN64 && !defined(__APPLE__)
    static char str[] = "PATH";
    char *s;

    asm volatile("push %%rdi; push %%rdi; mov %0, %%rdi;"
#if 1 && !defined(__MCC__) && (defined(__PIC__) || defined(__PIE__))
                 "call getenv@plt;"
#else
                 "call getenv;"
#endif
                 "pop %%rdi; pop %%rdi"
                 : "=a"(s) : "r"(str));
    printf("asmd: %s\n", s);
#endif
}

#if defined __x86_64__
#define RX "(%rip)"
#else
#define RX
#endif

void asm_dot_test(void) {
#ifndef __APPLE__
    int x;
    for (x = 1;; ++x) {
        int r = x;
        switch (x) {
        case 1:
            asm(".text; lea S" RX ",%eax; lea ." RX ",%ecx; sub %ecx,%eax; S=.; jmp p0");
        case 2:
#ifndef __clang__

            asm(".text; jmp .+6; .int 123; mov .-4" RX ",%eax; jmp p0");
#else
            asm(".text; mov $123, %eax; jmp p0");
#endif
        case 3:
#if !defined(_WIN32) && !defined(__clang__)
            asm(".pushsection \".data\"; Y=.; .int 999; X=Y; .int 456; X=.-4; .popsection");
#else
            asm(".data; Y=.; .int 999; X=Y; .int 456; X=.-4; .text");
#endif
            asm(".text; mov X" RX ",%eax; jmp p0");
        case 4:
#ifdef __clang__

            asm(".text; mov $789,%eax; jmp p0");
#else
#ifndef _WIN32
            asm(".data; X=.; .int 789; Y=.; .int 999; .previous");
#else
            asm(".data; X=.; .int 789; Y=.; .int 999; .text");
#endif
            asm(".text; mov X" RX ",%eax; X=Y; jmp p0");
#endif
        case 0:
            asm(".text; p0=.; mov %%eax,%0;" : "=m"(r));
            break;
        }
        if (r == x)
            break;
        printf("asm_dot_test %d: %d\n", x, r);
    }
#endif
}

void asm_pcrel_test(void) {
    unsigned o1, o2;

    asm("1: mov $2f-1b,%%eax; mov %%eax,%0" : "=m"(o1));

    asm("2: lea 2b" RX ",%eax; lea 1b" RX ",%ecx; sub %ecx,%eax");
    asm("mov %%eax,%0" : "=m"(o2));
    printf("%s : %x\n", __FUNCTION__, o1 - o2);
}

void asm_test(void) {
    char buf[128];
    unsigned int val, val2;
    struct struct123 s1;
    struct struct1231 s2 = {(word)&s1};

    int base_func = 42;
    void override_func3(void);
    word asmret;
#ifdef BOOL_ISOC99
    _Bool somebool;
#endif
    register int regvar asm("%esi");

    asm volatile("mov $0x1E-1,%eax");

    asm volatile("xorl %eax, %eax");

    memcpy1(buf, "hello", 6);
    strncat1(buf, " worldXXXXX", 3);
    printf("%s\n", buf);

    memcpy2(buf, "hello", 6);
    strncat2(buf, " worldXXXXX", 3);
    printf("%s\n", buf);

    printf("mul64=0x%Lx\n", mul64(0x12345678, 0xabcd1234));
    printf("inc64=0x%Lx\n", inc64(0x12345678ffffffff));

    s1.a = 42;
    s1.b = 43;
    printf("mconstraint: %d", mconstraint_test(&s2));
    printf(" %d %d\n", s1.a, s1.b);
    other_constraints_test();
    set = 0xff;
    sigdelset1(&set, 2);
    sigaddset1(&set, 16);

    goto label1;
label2:
    __asm__("btsl %1,%0" : "=m"(set) : "Ir"(20) : "cc");
    printf("set=0x%x\n", set);
    val = 0x01020304;
    printf("swab32(0x%08x) = 0x%0x\n", val, swab32(val));
#ifndef _WIN32
#ifndef __APPLE__
    override_func1();
    override_func2();

    asm volatile(".weak override_func3\n.set override_func3, base_func");
    override_func3();
    printf("asmstr: %s\n", get_asm_string());
    asm_local_label_diff();
#endif
#endif
    asm_local_statics();
#ifndef __clang__

    asm volatile("" : "=r"(asmret) : "0"(s2));
    if (asmret != s2.addr)
        printf("asmstr: failed\n");
#endif
#ifdef BOOL_ISOC99

    asm volatile("cmp %1,%2; sete %0" : "=a"(somebool) : "r"(1), "r"(2));
    if (!somebool)
        printf("asmbool: failed\n");
#endif
    val = 43;
    fancy_copy(&val, &val2);
    printf("fancycpy(%d)=%d\n", val, val2);
    val = 44;
    fancy_copy2(&val, &val2);
    printf("fancycpy2(%d)=%d\n", val, val2);
    asm volatile("mov $0x4243, %%esi" : "=r"(regvar));
    printf("regvar=%x\n", regvar);
    test_high_clobbers();
    trace_console(8, 8);
    test_asm_dead_code();
    test_asm_call();
    asm_dot_test();
    asm_pcrel_test();
    return;
label1:
    goto label2;
}

#else

void asm_test(void) {
}

#endif

#define COMPAT_TYPE(type1, type2)                                             \
    {                                                                         \
        printf("__builtin_types_compatible_p(%s, %s) = %d\n", #type1, #type2, \
               __builtin_types_compatible_p(type1, type2));                   \
    }

int constant_p_var;

int func(void);

static void builtin_test_bits(unsigned long long x, int cnt[]) {
#if GCC_MAJOR >= 4
    cnt[0] += __builtin_ffs(x);
    cnt[1] += __builtin_ffsl(x);
    cnt[2] += __builtin_ffsll(x);

    if ((unsigned int)x)
        cnt[3] += __builtin_clz(x);
    if ((unsigned long)x)
        cnt[4] += __builtin_clzl(x);
    if ((unsigned long long)x)
        cnt[5] += __builtin_clzll(x);

    if ((unsigned int)x)
        cnt[6] += __builtin_ctz(x);
    if ((unsigned long)x)
        cnt[7] += __builtin_ctzl(x);
    if ((unsigned long long)x)
        cnt[8] += __builtin_ctzll(x);

#if GCC_MAJOR >= 6 && (CC_NAME != CC_clang || GCC_MAJOR >= 11)

    cnt[9] += __builtin_clrsb(x);
    cnt[10] += __builtin_clrsbl(x);
    cnt[11] += __builtin_clrsbll(x);
#endif

    cnt[12] += __builtin_popcount(x);
    cnt[13] += __builtin_popcountl(x);
    cnt[14] += __builtin_popcountll(x);

    cnt[15] += __builtin_parity(x);
    cnt[16] += __builtin_parityl(x);
    cnt[17] += __builtin_parityll(x);
#endif
}
