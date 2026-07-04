void macro_test(void) {
    pf("N=%d\n", N);
    printf("aaa=%d\n", AAA);

    printf("min=%d\n", min(1, min(2, -1)));

    printf("s1=%s\n", glue(HIGH, LOW));
    printf("s2=%s\n", xglue(HIGH, LOW));
    printf("s3=%s\n", str("c"));
    printf("s4=%s\n", str(a1));
    printf("B3=%d\n", B3);

    printf("onetwothree=%d\n", onetwothree);

#ifdef A
    printf("A defined\n");
#endif
#ifdef B
    printf("B defined\n");
#endif
#ifdef A
    printf("A defined\n");
#else
    printf("A not defined\n");
#endif
#ifdef B
    printf("B defined\n");
#else
    printf("B not defined\n");
#endif

#ifdef A
    printf("A defined\n");
#ifdef B
    printf("B1 defined\n");
#else
    printf("B1 not defined\n");
#endif
#else
    printf("A not defined\n");
#ifdef B
    printf("B2 defined\n");
#else
    printf("B2 not defined\n");
#endif
#endif

#if 1 + 1
    printf("test true1\n");
#endif
#if 0
    printf("test true2\n");
#endif
#if 1 - 1
    printf("test true3\n");
#endif
#if defined(A)
    printf("test trueA\n");
#endif
#if defined(B)
    printf("test trueB\n");
#endif

#if 0
    printf("test 0\n");
#elif 0
    printf("test 1\n");
#elif 2
    printf("test 2\n");
#else
    printf("test 3\n");
#endif

    MACRO_NOARGS();

    printf("%d\n", TEST_CALL(TEST_CONST));

#ifdef C99_MACROS
    printf("__func__ = %s\n", __func__);
    dprintf(1, "vaarg=%d\n", 1);
#endif
    dprintf1(1, "vaarg1\n");
    dprintf1(1, "vaarg1=%d\n", 2);
    dprintf1(1, "vaarg1=%d %d\n", 1, 2);

    printf("func='%s'\n", __FUNCTION__);

    printf("INT64_MIN=" LONG_LONG_FORMAT "\n", INT64_MIN);
    {
        int a;
        a = 1;
        glue(a +, +);
        printf("a=%d\n", a);
        glue(a <, <= 2);
        printf("a=%d\n", a);
    }

#define MF_s MF_hello
#define MF_hello(msg) printf("%s\n", msg)

#define MF_t             \
    printf("tralala\n"); \
    MF_hello

    MF_s("hi");
    MF_t("hi");

    printf("qq=%d\n", qq(qq)(2));

#define qq1(x) 1
    printf("qq1=%d\n", qq1());

    TEST2();

    TEST2();

    printf("basefromheader %s\n", get_basefile_from_header());
    printf("base %s\n", __BASE_FILE__);
#if !(defined _WIN32 && CC_NAME == CC_clang)
    {

        const char *fn = get_file_from_header();
        if (fn[0] == '.' && fn[1] == '/')
            fn += 2;
        printf("filefromheader %s\n", fn);
    }
#endif

    printf("file %s\n", __FILE__);

    have_included_42test_h = 1;
    have_included_42test_h_second = 1;
    have_included_42test_h_third = 1;

    printf("print a backslash: %s\n", stringify(\\));
}

static void print_num(char *fn, int line, int num) {
    printf("fn %s, line %d, num %d\n", fn, line, num);
}

void recursive_macro_test(void) {

#define ELF32_ST_TYPE(val) ((val) & 0xf)
#define ELF32_ST_INFO(bind, type) (((bind) << 4) + ((type) & 0xf))
#define STB_WEAK 2
#define ELFW(type) ELF##32##_##type
    printf("%d\n", ELFW(ST_INFO)(STB_WEAK, ELFW(ST_TYPE)(123)));

#define WRAP(x) x

#define print_num(x) print_num(__FILE__, __LINE__, x)
    print_num(123);
    WRAP(print_num(123));
    WRAP(WRAP(print_num(123)));

    static struct recursive_macro {
        int rm_field;
    } G;
#define rm_field (G.rm_field)
    printf("rm_field = %d\n", rm_field);
    printf("rm_field = %d\n", WRAP(rm_field));
    WRAP((printf("rm_field = %d %d\n", rm_field, WRAP(rm_field))));
}

#if __MCC__
int op(a, b) {
    return a / b;
}

int ret(a) {
    if (a == 2)
        return 1;
    if (a == 3)
        return 2;
    return 0;
}
#endif

#if !defined(__MCC__) && (__GNUC__ >= 8)

#define CONSTANTINDEXEDSTRLIT
#endif
char str_ag1[] = "b";
char str_ag2[] = {"b"};

#ifdef CONSTANTINDEXEDSTRLIT
char str_ag3[] = {"ab"[1], 0};
char str_x[2] = {"xy"
                 "z"[2],
                 0};
#endif
char *str_ar[] = {"one", "two"};
struct str_SS {
    unsigned char a[3], b;
};
struct str_SS str_sinit15 = {"r"};
struct str_SS str_sinit16[] = {{"q"}, 2};

static void string_test2() {
    char *p = "hello";
    char a3[2] = {"p"};
    char a4[2] = {"ab"
                  "c"[2],
                  0};
    char *pa1 = "def" + 1;
    char *pa2 = {"xyz" + 1};
    int i = 0;
    struct str_SS ss = {{[0 ... 1] = 'a'}, 0};
#ifndef CONSTANTINDEXEDSTRLIT
    char str_ag3[] = {"ab"[1], 0};
    char str_x[2] = {"xy"
                     "z"[2],
                     0};
#endif
    puts("string_test2");
    puts(str_ag1);
    puts(str_ag2);

    puts(str_ag3);
    puts(str_x);
    puts(str_sinit15.a);
    puts(str_sinit16[0].a);
    puts(a3);
    puts(a4);
    puts(p);
    puts("world");
    printf("%s\n", "bla");
    puts(str_ar[0]);
    puts(str_ar[1]);
    puts(ss.a);
    puts(i >= 0 ? "one" : "two");
    puts(pa1);
    puts(pa2);
}

void ps(const char *s) {
    int c;
    while (1) {
        c = *s;
        if (c == 0)
            break;
        printf("%c", c);
        s++;
    }
}

const char foo1_string[] = "\
bar\n\
test\14\
1";

void string_test() {
    unsigned int b;
    printf("string:\n");
    printf("\141\1423\143\n");
    printf("\x41\x42\x43\x3a\n");
    printf("c=%c\n", 'r');
    printf("wc=%C 0x%lx %C\n", L'a', L'\x1234', L'c');
    printf("foo1_string='%s'\n", foo1_string);
#if 0
    printf("wstring=%S\n", L"abc");
    printf("wstring=%S\n", L"abc" L"def" "ghi");
    printf("'\\377'=%d '\\xff'=%d\n", '\377', '\xff');
    printf("L'\\377'=%d L'\\xff'=%d\n", L'\377', L'\xff');
#endif
    ps("test\n");
    b = 32;
    while ((b = b + 1) < 96) {
        printf("%c", b);
    }
    printf("\n");
    printf("fib=%d\n", fib(33));
    b = 262144;
    while (b != 0x80000000) {
        num(b);
        b = b * 2;
    }
    string_test2();
}

void if1t(int n, int a, int b, int c) {
    if (a && b)
        printf("if1t: %d 1 %d %d\n", n, a, b);
    if (a && !b)
        printf("if1t: %d 2 %d %d\n", n, a, b);
    if (!a && b)
        printf("if1t: %d 3 %d %d\n", n, a, b);
    if (!a && !b)
        printf("if1t: %d 4 %d %d\n", n, a, b);
    if (a || b)
        printf("if1t: %d 5 %d %d\n", n, a, b);
    if (a || !b)
        printf("if1t: %d 6 %d %d\n", n, a, b);
    if (!a || b)
        printf("if1t: %d 7 %d %d\n", n, a, b);
    if (!a || !b)
        printf("if1t: %d 8 %d %d\n", n, a, b);
    if (a && b || c)
        printf("if1t: %d 9 %d %d %d\n", n, a, b, c);
    if (a || b && c)
        printf("if1t: %d 10 %d %d %d\n", n, a, b, c);
    if (a > b - 1 && c)
        printf("if1t: %d 11 %d %d %d\n", n, a, b, c);
    if (a > b - 1 || c)
        printf("if1t: %d 12 %d %d %d\n", n, a, b, c);
    if (a > 0 && 1)
        printf("if1t: %d 13 %d %d %d\n", n, a, b, c);
    if (a > 0 || 0)
        printf("if1t: %d 14 %d %d %d\n", n, a, b, c);
}

void if2t(void) {
    if (0 && 1 || printf("if2t:ok\n") || 1)
        printf("if2t:ok2\n");
    printf("if2t:ok3\n");
}

void if3t(void) {
    volatile long long i = 1;
    if (i <= 18446744073709551615ULL)
        ;
    else
        printf("if3t:wrong 1\n");
}
