/* Legacy tcctest body — legacy_args. Aggregate-only unit: it shares file-scope
   macros/globals with the other legacy_*.h parts and full_language.c's
   prelude, so it is #included (in order) into full_language.c and is not a
   standalone parts-suite unit. Extracted verbatim to keep gcc==mcc. */
void manyarg_test(void)
{
    LONG_DOUBLE ld = 1234567891234LL;
    printf("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f\n",
           1, 2, 3, 4, 5, 6, 7, 8,
           0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0);
    printf("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
           LONG_LONG_FORMAT " " LONG_LONG_FORMAT " %f %f\n",
           1, 2, 3, 4, 5, 6, 7, 8,
           0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
           1234567891234LL, 987654321986LL,
           42.0, 43.0);
    printf("%Lf %d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
           LONG_LONG_FORMAT " " LONG_LONG_FORMAT " %f %f\n",
           ld, 1, 2, 3, 4, 5, 6, 7, 8,
           0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
           1234567891234LL, 987654321986LL,
           42.0, 43.0);
    printf("%d %d %d %d %d %d %d %d %Lf\n",
           1, 2, 3, 4, 5, 6, 7, 8, ld);
    printf("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
           LONG_LONG_FORMAT " " LONG_LONG_FORMAT "%f %f %Lf\n",
           1, 2, 3, 4, 5, 6, 7, 8,
           0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
           1234567891234LL, 987654321986LL,
           42.0, 43.0, ld);
    printf("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
           "%Lf " LONG_LONG_FORMAT " " LONG_LONG_FORMAT " %f %f %Lf\n",
           1, 2, 3, 4, 5, 6, 7, 8,
           0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
           ld, 1234567891234LL, 987654321986LL,
           42.0, 43.0, ld);
}

void*
va_arg_with_struct_ptr(va_list ap) {





        struct X { int _x; };
        struct X *x = va_arg(ap, struct X *);
        return x;
}

void vprintf1(const char *fmt, ...)
{
    va_list ap, aq;
    const char *p;
    int c, i;
    double d;
    long long ll;
    LONG_DOUBLE ld;

    va_start(aq, fmt);
    va_copy(ap, aq);

    p = fmt;
    for(;;) {
        c = *p;
        if (c == '\0')
            break;
        p++;
        if (c == '%') {
            c = *p;
            switch(c) {
            case '\0':
                goto the_end;
            case 'd':
                i = va_arg(ap, int);
                printf("%d", i);
                break;
            case 'f':
                d = va_arg(ap, double);
                printf("%f", d);
                break;
            case 'l':
                ll = va_arg(ap, long long);
                printf(LONG_LONG_FORMAT, ll);
                break;
            case 'F':
                ld = va_arg(ap, LONG_DOUBLE);
                printf("%Lf", ld);
                break;
            }
            p++;
        } else {
            putchar(c);
        }
    }
 the_end:
    va_end(aq);
    va_end(ap);
}

struct myspace {
    short int profile;
};
struct myspace2 {
#if CC_NAME == CC_clang
    char a[1];
#else
    char a[0];
#endif
};
struct myspace3 {
    char a[1];
};
struct myspace4 {
    char a[2];
};
struct mytest {
    void *foo, *bar, *baz;
};

struct mytest stdarg_for_struct(struct myspace bob, ...)
{
    struct myspace george, bill;
    struct myspace2 alex1;
    struct myspace3 alex2;
    struct myspace4 alex3;
    va_list ap;
    short int validate;

    va_start(ap, bob);
    alex1    = va_arg(ap, struct myspace2);
    alex2    = va_arg(ap, struct myspace3);
    alex3    = va_arg(ap, struct myspace4);
    bill     = va_arg(ap, struct myspace);
    george   = va_arg(ap, struct myspace);
    validate = va_arg(ap, int);
    printf("stdarg_for_struct: %d %d %d %d %d %d %d\n",
           alex2.a[0], alex3.a[0], alex3.a[1],
           bob.profile, bill.profile, george.profile, validate);
    va_end(ap);
    return (struct mytest) {};
}

void stdarg_for_libc(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void stdarg_syntax(int n, ...)
{
    int i;
    va_list ap;
    if (1)
      va_start(ap, n);
    else
      ;
    i = va_arg(ap, int);
    printf("stdarg_void_expr: %d\n", i);
    (va_end(ap));
}

typedef struct{
    double x,y;
} point;
point pts[]={{1.0,2.0},{3.0,4.0},{5.0,6.0},{7.0,8.0},{9.0,10.0},{11.0,12.0}};

static void stdarg_double_struct(int nargs, int posd,...)
{
    int i;
    double d;
    point pi;
    va_list args;

    printf ("stdarg_double_struct: %d\n", posd);
    va_start(args,posd);
    for(i = 0; i < nargs; i++) {
        if (i == posd) {
            d = va_arg (args, double);
            printf ("d %d = %g\n", i, d);
        }
        else {
            pi = va_arg (args, point);
            printf ("pts[%d] = %g %g\n", i, pi.x, pi.y);
        }
    }
    va_end(args);
}

void stdarg_test(void)
{
    LONG_DOUBLE ld = 1234567891234LL;
    struct myspace bob;
    struct myspace2 bob2;
    struct myspace3 bob3;
    struct myspace4 bob4;

    vprintf1("%d %d %d\n", 1, 2, 3);
    vprintf1("%f %d %f\n", 1.0, 2, 3.0);
    vprintf1("%l %l %d %f\n", 1234567891234LL, 987654321986LL, 3, 1234.0);
    vprintf1("%F %F %F\n", LONG_DOUBLE_LITERAL(1.2), LONG_DOUBLE_LITERAL(2.3), LONG_DOUBLE_LITERAL(3.4));
    vprintf1("%d %f %l %F %d %f %l %F\n",
             1, 1.2, 3LL, LONG_DOUBLE_LITERAL(4.5), 6, 7.8, 9LL, LONG_DOUBLE_LITERAL(0.1));
    vprintf1("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f\n",
             1, 2, 3, 4, 5, 6, 7, 8,
             0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8);
    vprintf1("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f\n",
             1, 2, 3, 4, 5, 6, 7, 8,
             0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0);
    vprintf1("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
             "%l %l %f %f\n",
             1, 2, 3, 4, 5, 6, 7, 8,
             0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
             1234567891234LL, 987654321986LL,
             42.0, 43.0);
    vprintf1("%F %d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
             "%l %l %f %f\n",
             ld, 1, 2, 3, 4, 5, 6, 7, 8,
             0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
             1234567891234LL, 987654321986LL,
             42.0, 43.0);
    vprintf1("%d %d %d %d %d %d %d %d %F\n",
             1, 2, 3, 4, 5, 6, 7, 8, ld);
    vprintf1("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
             "%l %l %f %f %F\n",
             1, 2, 3, 4, 5, 6, 7, 8,
             0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
             1234567891234LL, 987654321986LL,
             42.0, 43.0, ld);
    vprintf1("%d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f %f %f "
             "%F %l %l %f %f %F\n",
             1, 2, 3, 4, 5, 6, 7, 8,
             0.1, 1.2, 2.3, 3.4, 4.5, 5.6, 6.7, 7.8, 8.9, 9.0,
             ld, 1234567891234LL, 987654321986LL,
             42.0, 43.0, ld);

    bob.profile = 42;
    bob3.a[0] = 1;
    bob4.a[0] = 2;
    bob4.a[1] = 3;
    stdarg_for_struct(bob, bob2, bob3, bob4, bob, bob, bob.profile);
    stdarg_for_libc("stdarg_for_libc: %s %.2f %d\n", "string", 1.23, 456);
    stdarg_syntax(1, 17);
#if !(defined _WIN32 && CC_NAME == CC_clang)
    stdarg_double_struct(6,-1,pts[0],pts[1],pts[2],pts[3],pts[4],pts[5]);
    stdarg_double_struct(7,1,pts[0],-1.0,pts[1],pts[2],pts[3],pts[4],pts[5]);
    stdarg_double_struct(7,2,pts[0],pts[1],-1.0,pts[2],pts[3],pts[4],pts[5]);
    stdarg_double_struct(7,3,pts[0],pts[1],pts[2],-1.0,pts[3],pts[4],pts[5]);
    stdarg_double_struct(7,4,pts[0],pts[1],pts[2],pts[3],-1.0,pts[4],pts[5]);
    stdarg_double_struct(7,5,pts[0],pts[1],pts[2],pts[3],pts[4],-1.0,pts[5]);
#endif
}

int reltab[3] = { 1, 2, 3 };

int *rel1 = &reltab[1];
int *rel2 = &reltab[2];

void getmyaddress(void)
{
    printf("in getmyaddress\n");
}

#ifdef __LP64__
long __pa_symbol(void)
{





       return ((long)(((unsigned long)(&rel1))) - (0xffffffff80000000UL));
}
#endif

uintptr_t theaddress = (uintptr_t)getmyaddress;
void relocation_test(void)
{
    void (*fptr)(void) = (void (*)(void))theaddress;
    printf("*rel1=%d\n", *rel1);
    printf("*rel2=%d\n", *rel2);
    fptr();
#ifdef __LP64__

    printf("pa_symbol: %d\n", (long)&rel1 == __pa_symbol() - 0x80000000);
#endif
}

void old_style_f(a,b,c)
     int a, b;
     double c;
{
    printf("a=%d b=%d b=%f\n", a, b, c);
}

void decl_func1(int cmpfn())
{
    printf("cmpfn=%lx\n", (long)cmpfn);
}

void decl_func2(cmpfn)
int cmpfn();
{
    printf("cmpfn=%lx\n", (long)cmpfn);
}

void old_style_function_test(void)
{
#if CC_NAME == CC_clang




    old_style_f(1, 2, 3.0);
#else
    old_style_f((void *)1, 2, 3.0);
#endif
    decl_func1(NULL);
    decl_func2(NULL);
}

void alloca_test()
{
    char *p = alloca(16);
    strcpy(p,"123456789012345");
    printf("alloca: p is %s\n", p);
    char *demo = "This is only a test.\n";

    printf("alloca: %s\n", strcpy(alloca(strlen(demo)+1),demo) );
}

void *bounds_checking_is_enabled()
{
    char ca[10], *cp = ca-1;
    return (ca != cp + 1) ? cp : NULL;
}

typedef int constant_negative_array_size_as_compile_time_assertion_idiom[(1 ? 2 : 0) - 1];

void c99_vla_test_1(int size1, int size2)
{
    int size = size1 * size2;
    int tab1[size][2], tab2[10][2];
    void *tab1_ptr, *tab2_ptr, *bad_ptr;



    size = size-1;

    printf("Test C99 VLA 1 (sizeof): ");
    printf("%s\n", (sizeof tab1 == size1 * size2 * 2 * sizeof(int)) ? "PASSED" : "FAILED");
    tab1_ptr = tab1;
    tab2_ptr = tab2;
    printf("Test C99 VLA 2 (ptrs subtract): ");
    printf("%s\n", (tab2 - tab1 == (tab2_ptr - tab1_ptr) / (sizeof(int) * 2)) ? "PASSED" : "FAILED");
    printf("Test C99 VLA 3 (ptr add): ");
    printf("%s\n", &tab1[5][1] == (tab1_ptr + (5 * 2 + 1) * sizeof(int)) ? "PASSED" : "FAILED");
    printf("Test C99 VLA 4 (ptr access): ");
    tab1[size1][1] = 42;
    printf("%s\n", (*((int *) (tab1_ptr + (size1 * 2 + 1) * sizeof(int))) == 42) ? "PASSED" : "FAILED");

    printf("Test C99 VLA 5 (bounds checking (might be disabled)): ");
    if (bad_ptr = bounds_checking_is_enabled()) {
        int *t1 = &tab1[size1 * size2 - 1][3];
        int *t2 = &tab2[9][3];
        printf("%s ", bad_ptr == t1 ? "PASSED" : "FAILED");
        printf("%s ", bad_ptr == t2 ? "PASSED" : "FAILED");

        char*c1 = 1 + sizeof(tab1) + (char*)tab1;
        char*c2 = 1 + sizeof(tab2) + (char*)tab2;
        printf("%s ", bad_ptr == c1 ? "PASSED" : "FAILED");
        printf("%s ", bad_ptr == c2 ? "PASSED" : "FAILED");

        int *i1 = tab1[-1];
        int *i2 = tab2[-1];
        printf("%s ", bad_ptr == i1 ? "PASSED" : "FAILED");
        printf("%s ", bad_ptr == i2 ? "PASSED" : "FAILED");

        int *x1 = tab1[size1 * size2 + 1];
        int *x2 = tab2[10 + 1];
        printf("%s ", bad_ptr == x1 ? "PASSED" : "FAILED");
        printf("%s ", bad_ptr == x2 ? "PASSED" : "FAILED");
    } else {
        printf("PASSED PASSED PASSED PASSED PASSED PASSED PASSED PASSED ");
    }
    printf("\n");
}

void c99_vla_test_2(int d, int h, int w)
{
    int x, y, z;
    int (*arr)[h][w] = malloc(sizeof(int) * d*h*w);
    int c = 1;
    static int (*starr)[h][w];

    printf("Test C99 VLA 6 (pointer)\n");

    for (z=0; z<d; z++) {
        for (y=0; y<h; y++) {
            for (x=0; x<w; x++) {
                arr[z][y][x] = c++;
            }
        }
    }
    for (z=0; z<d; z++) {
        for (y=0; y<h; y++) {
            for (x=0; x<w; x++) {
                printf(" %2d", arr[z][y][x]);
            }
            puts("");
        }
        puts("");
    }
    starr = &arr[1];
    printf(" sizes : %d %d %d\n"
           " pdiff : %d %d\n"
           " tests : %d %d %d\n",
        sizeof (*arr), sizeof (*arr)[0], sizeof (*arr)[0][0],
        arr + 2 - arr, *arr + 3 - *arr,
        0 == sizeof (*arr + 1) - sizeof arr,
        0 == sizeof sizeof *arr - sizeof arr,
        starr[0][2][3] == arr[1][2][3]
        );
    free (arr);
}

void c99_vla_test_3a (int arr[2][3][4])
{
    printf ("%d\n", arr[1][2][3]);
}

void c99_vla_test_3b(int s, int arr[s][3][4])
{
    printf ("%d\n", arr[1][2][3]);
}

void c99_vla_test_3c(int s, int arr[2][s][4])
{
    printf ("%d\n", arr[1][2][3]);
}

void c99_vla_test_3d(int s, int arr[2][3][s])
{
    printf ("%d\n", arr[1][2][3]);
}

void c99_vla_test_3e(int s, int arr[][3][--s])
{
    printf ("%d %d %d\n", sizeof arr, s, arr[1][2][3]);
}

void c99_vla_test_3(void)
{
    int a[2][3][4];

    memset (a, 0, sizeof(a));
    a[1][2][3] = 123;
    c99_vla_test_3a(a);
    c99_vla_test_3b(2, a);
    c99_vla_test_3c(3, a);
    c99_vla_test_3d(4, a);
    c99_vla_test_3e(5, a);
}

void c99_vla_test(void)
{
    c99_vla_test_1(5, 2);
    c99_vla_test_2(3, 4, 5);
    c99_vla_test_3();
}


