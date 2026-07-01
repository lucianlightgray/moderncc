/* C9911 §6.3 Conversions (s6_3) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_3.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
static int s6_3_taken;
static int s6_3_side(int v){ s6_3_taken = v; return v; }

void s6_3_conversions(void)
{
    /* §6.3.1.2p1 scalar->_Bool: 0 iff compares equal to 0 */
    printf("bool: %d %d %d %d %d\n",
        (int)(_Bool)2.5, (int)(_Bool)0.0, (int)(_Bool)-0.0,
        (int)(_Bool)(void*)0, (int)(_Bool)(char)256);

    /* §6.3.1.3p2 unsigned conversion reduces modulo 2^N */
    printf("umod: %d %lu %d\n",
        (int)(unsigned char)257, (unsigned long)((unsigned)-1) & 0xFFFFFFFFul,
        (int)(unsigned short)65537);

    /* §6.3.1.3p3 signed narrowing (impl-defined; gcc==clang==mcc take low bits) */
    printf("snarrow: %d %d\n", (int)(signed char)300, (int)(signed char)-200);

    /* §6.3.1.4p1 real float->int truncates toward zero */
    printf("ftrunc: %d %d %d %d\n",
        (int)3.9, (int)-3.9, (int)0.99, (int)-0.99);

    /* §6.3.1.4p2 int->float exactly representable is preserved */
    printf("i2f: %d %d\n", (double)16777216 == 16777216.0, (float)1 == 1.0f);

    /* §6.3.1.5p1 widening float->double preserves value */
    printf("widen: %d %d\n",
        (double)(float)1.0f == 1.0, (double)(float)0.5f == 0.5);

    /* §6.3.1.5p2 narrowing double->float can be inexact */
    printf("narrow: %d\n", (float)0.1 != 0.1);

    /* §6.3.1.7p2 complex->real discards imaginary part */
    {
        double _Complex z = 5.0 + 9.0*I;
        printf("c2r: %d\n", (double)z == 5.0);
    }
    /* §6.3.1.7p1 real->complex: imaginary part is +0 */
    {
        double _Complex z = 7.0;
        printf("r2c: %d %d\n", creal(z) == 7.0, cimag(z) == 0.0);
    }

    /* §6.3.1.8p1 usual arithmetic conversions across signedness */
    printf("uac: %d %d\n",
        (-1 < 0u),          /* false: -1 converts to unsigned int */
        ((long)-1 < 0u));   /* true on LP64: long represents all unsigned int */

    /* §6.3.1.8p1 same-sign lesser-rank operand converts up */
    {
        long long big = 0x100000000LL;
        int i = 1;
        printf("rank: %lld\n", big + i);
    }

    /* §6.3.2.1p3 array decays to pointer to first element */
    {
        int a[3] = {11, 22, 33};
        int *p = a;
        printf("decay: %d %d\n", *p, sizeof(a) == 3 * sizeof(int));
    }

    /* §6.3.2.1p4 function designator decays to function pointer */
    {
        int (*fp)(int) = s6_3_side;
        printf("fdecay: %d\n", fp(42));
    }

    /* §6.3.2.2p1 cast to void discards value but keeps side effects */
    s6_3_taken = 0;
    (void)s6_3_side(99);
    printf("void: %d\n", s6_3_taken);

    /* §6.3.2.3p3/p4 null pointer constants */
    {
        int obj = 0;
        int *np = 0;
        printf("null: %d %d\n", np == (void*)0, ((void*)0 == (char*)0));
        printf("nulldiff: %d\n", np != &obj);
    }

    /* §6.3.2.3p1 void* <-> object pointer round trip */
    {
        int obj = 5;
        void *vp = &obj;
        int *ip = vp;
        printf("voidp: %d\n", ip == &obj);
    }

    /* §6.3.2.3p7 pointer to char aliases lowest byte; byte walk in order */
    {
        unsigned int u = 0x04030201u;
        unsigned char *b = (unsigned char *)&u;
        printf("bytewalk: %d %d\n",
            (void *)b == (void *)&u,
            (b[0] + b[1] + b[2] + b[3]) == 10);
    }
}
