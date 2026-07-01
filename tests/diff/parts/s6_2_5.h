/* C9911 §6.2.5-§6.2.8 Types, representations, alignment (s6_2_5) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_2_5.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
#define s6_2_5_gtag(x) _Generic((x), \
    char: 1, signed char: 2, unsigned char: 3, default: 0)

enum s6_2_5_e { s6_2_5_A, s6_2_5_B, s6_2_5_C };

extern int s6_2_5_arr[];
int s6_2_5_arr[5];

void s6_2_5_types(void)
{
    /* §6.2.5p2 : any scalar -> _Bool stores 1 for every nonzero incl. float */
    printf("bool: %d %d %d %d %d\n",
           (int)(_Bool)5, (int)(_Bool)0, (int)(_Bool)0.3,
           (int)(_Bool)0.0, (int)(_Bool)-2);

    /* §6.2.5p3 : char, signed char, unsigned char are three distinct types */
    {
        char c = 0; signed char sc = 0; unsigned char uc = 0;
        printf("chardistinct: %d %d %d\n",
               s6_2_5_gtag(c), s6_2_5_gtag(sc), s6_2_5_gtag(uc));
        printf("charsize: %d %d %d\n",
               (int)sizeof(char), (int)sizeof(signed char),
               (int)sizeof(unsigned char));
    }

    /* §6.2.5p9 : unsigned arithmetic is modular (never overflows) */
    {
        unsigned u = UINT_MAX;
        unsigned char ucv = 255;
        printf("umod: %d %d\n",
               (u + 1u) == 0u,
               (unsigned char)(ucv + 1) == 0);
    }

    /* §6.2.5p9 / §6.2.6.2p4 : nonneg signed shares rep with unsigned */
    {
        int i = 1234567;
        unsigned u;
        memcpy(&u, &i, sizeof u);
        printf("nonnegrep: %d\n", u == (unsigned)i);
    }

    /* §6.2.6.1p2/p4 : object representation is sizeof(T) bytes, byte-copyable */
    {
        int i = 0x1234ABCD, j;
        unsigned char b[sizeof(int)];
        memcpy(b, &i, sizeof i);
        memcpy(&j, b, sizeof j);
        printf("objrep: %d %d\n", (int)sizeof b == (int)sizeof(int), i == j);
    }

    /* §6.2.6.2p2 : two's complement on these targets */
    printf("twoscomp: %d\n", INT_MIN == -INT_MAX - 1);

    /* §6.2.6.1p3 / §6.2.6.2p1 : unsigned char pure binary, no padding */
    printf("ucharmax: %d %d\n", UCHAR_MAX == (1 << CHAR_BIT) - 1, CHAR_BIT == 8);

    /* §6.2.5p13 : complex laid out as real[2], real part first */
    {
        double _Complex cd;
        double a[2];
        __real__ cd = 3.0;
        __imag__ cd = 4.0;
        memcpy(a, &cd, sizeof a);
        printf("cplxlayout: %d %d %d\n",
               sizeof(double _Complex) == sizeof(double[2]),
               a[0] == 3.0, a[1] == 4.0);
    }

    /* §6.2.8p1/p4/p6 : alignments */
    printf("align: %d %d %d\n",
           _Alignof(char) == 1,
           _Alignof(float[2]) == _Alignof(float),
           _Alignof(double _Complex) == _Alignof(double));

    /* §6.2.8p2 : max_align_t is a fundamental alignment >= double */
    printf("maxalign: %d\n", _Alignof(max_align_t) >= _Alignof(double));

    /* §6.2.8p3/p5 : _Alignas over-alignment honored at runtime */
    {
        _Alignas(16) unsigned char buf[16];
        printf("overalign: %d\n", ((unsigned long)(size_t)&buf[0] % 16u) == 0);
    }

    /* §6.2.8p5 : _Alignas(0) has no effect */
    {
        _Alignas(0) int x = 7;
        printf("alignaszero: %d %d\n", _Alignof(x) == _Alignof(int), x == 7);
    }

    /* §6.2.7p3 : composite adopts the known array bound */
    printf("composite: %d\n", (int)sizeof(s6_2_5_arr) == 5 * (int)sizeof(int));

    /* §6.2.5p16 / §6.2.7p1 : enum freely mixes with its integer type */
    {
        enum s6_2_5_e e = s6_2_5_B;
        int i = e;
        e = (enum s6_2_5_e)(i + 1);
        printf("enum: %d %d\n", i == 1, e == s6_2_5_C);
    }
}
