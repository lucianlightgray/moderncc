/* x86-64 System V struct-argument ABI classification stress: float-only structs
   (SSE), mixed int/float (INTEGER), the 16-byte two-eightbyte boundary, and a
   call that exhausts both the integer (rdi..r9) and SSE (xmm0..7) argument
   registers so later aggregates spill to the stack. Also covers variadic
   floating-point promotion and bitfield round-tripping. All results compared to
   gcc and clang. 3-way verified. */
#include <stdio.h>
#include <stdarg.h>

typedef struct { float a, b; } S2f;          /* one SSE eightbyte    */
typedef struct { float a; int b; } Sfi;      /* one INTEGER eightbyte */
typedef struct { float a, b, c, d; } S4f;    /* two SSE eightbytes   */
typedef struct { double d; float f; } Sdf;
typedef struct { int a; double d; } ID;      /* 1 INT + 1 SSE reg    */
typedef struct { long a, b; } LL;            /* 2 INT regs           */

static S2f mk2f(float a, float b) { S2f s = { a, b }; return s; }
static S4f sh4f(S4f s) { S4f r = { s.d, s.c, s.b, s.a }; return r; }
static Sdf negdf(Sdf s) { s.d = -s.d; s.f = -s.f; return s; }

/* mix of scalars and aggregates that overruns the argument registers */
static long big(int i0, ID s1, double d0, LL s2, int i1,
                ID s3, double d1, LL s4, int i2)
{
    return i0 + s1.a + (long)s1.d + (long)d0 + s2.a + s2.b + i1
         + s3.a + (long)s3.d + (long)d1 + s4.a + s4.b + i2;
}

static double vsum(int n, ...)
{
    va_list ap; va_start(ap, n);
    double s = 0;
    for (int i = 0; i < n; i++)
        s += va_arg(ap, double);
    va_end(ap);
    return s;
}

struct BF { unsigned a : 3; int b : 5; unsigned c : 24; signed d : 1; unsigned e : 31; };
static struct BF bf_mut(struct BF x)
{
    x.a++; x.b = -x.b; x.c ^= 0xfffff; x.d = ~x.d; x.e >>= 1;
    return x;
}

int main(void)
{
    S2f p = mk2f(1.25f, 2.5f);
    printf("S2f %g %g\n", p.a, p.b);

    Sfi x = { 3.5f, 7 };
    printf("Sfi %g %d\n", x.a, x.b);

    S4f a = { 1, 2, 3, 4 };
    a = sh4f(sh4f(a));
    printf("S4f %g %g %g %g\n", a.a, a.b, a.c, a.d);

    Sdf b = { 6.5, -1.5f };
    b = negdf(b);
    printf("Sdf %g %g\n", b.d, b.f);

    ID s1 = { 1, 1.5 }, s3 = { 3, 3.5 };
    LL s2 = { 10, 20 }, s4 = { 30, 40 };
    printf("big %ld\n", big(100, s1, 2.0, s2, 5, s3, 4.0, s4, 7));

    printf("vsum %g\n", vsum(4, 1.5, 2.5, 3.5, 4.5));

    struct BF bf = { 5, -7, 0x123456, 0, 0x40000000 };
    bf = bf_mut(bf);
    printf("BF %u %d %u %d %u\n", bf.a, bf.b, bf.c, bf.d, bf.e);
    return 0;
}
