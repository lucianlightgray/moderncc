/* Harder struct corners: a __attribute__((packed)) struct (no padding, validated
   by an exact byte dump), struct return through an indirect call via a function
   pointer, a compound literal passed by value with postfix member access, _Bool
   struct members, and a deeply recursive by-value pass. Compared to gcc/clang.
   3-way verified. */
#include <stdio.h>
#include <string.h>

typedef struct __attribute__((packed)) { char c; int i; double d; } Packed; /* 13B */
typedef struct { _Bool f1, f2; int n; } SB;
typedef struct { int x, y; } P;

static void dump(const char *t, const void *p, unsigned n)
{
    const unsigned char *b = p;
    printf("%s:", t);
    for (unsigned i = 0; i < n; i++)
        printf(" %02x", b[i]);
    printf("\n");
}

static P addp(P a, P b) { P r = { a.x + b.x, a.y + b.y }; return r; }
static P (*fp)(P, P) = addp;
static P viaptr(P a, P b) { return fp(a, b); }                 /* indirect call */
static int sumPacked(Packed p) { return p.c + p.i + (int)p.d; }
static SB flip(SB s) { s.f1 = !s.f1; s.f2 = !s.f2; s.n = -s.n; return s; }
static int deep(P p, int n) { if (!n) return p.x + p.y; P q = { p.x + 1, p.y - 1 }; return deep(q, n - 1); }

int main(void)
{
    Packed pk; memset(&pk, 0, sizeof pk); pk.c = 3; pk.i = 1000; pk.d = 2.5;
    printf("Packed sz=%u sum=%d\n", (unsigned)sizeof(Packed), sumPacked(pk));
    dump("Packed", &pk, (unsigned)sizeof pk);

    SB sb = { 1, 0, 42 };
    sb = flip(flip(sb)); /* double flip -> identity */
    printf("SB %d %d %d\n", sb.f1, sb.f2, sb.n);

    P a = { 3, 4 }, b = { 10, 20 };
    printf("viaptr %d %d\n", viaptr(a, b).x, viaptr(a, b).y);

    printf("clit %d\n",
           addp((P){ 1, 2 }, (P){ 3, 4 }).x + addp((P){ 1, 2 }, (P){ 3, 4 }).y);

    printf("deep %d\n", deep((P){ 0, 0 }, 1000));
    return 0;
}
