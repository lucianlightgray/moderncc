/* C9911 §6.8 Statements and blocks (s6_8) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_8.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
typedef struct { int a, b, c; } s6_8_S;
static s6_8_S s6_8_id(s6_8_S s) { return s; }
static double s6_8_retd(void) { return 3; }
static unsigned char s6_8_retc(void) { return 300; }
enum s6_8_E { s6_8_A, s6_8_B = 5, s6_8_C };

static int s6_8_dang(int a, int b) { int r = 0; if (a) if (b) r = 1; else r = 2; return r; }

static int s6_8_sw(int x) {
    int r = 0;
    switch (x) {
    case 1: r += 1;
    case 2: r += 20; break;
    default: r += 100;
    }
    return r;
}

static int s6_8_nomatch(void) {
    int r = 7;
    switch (3) { case 1: r = 99; case 2: r = 98; }
    return r;
}

static int s6_8_enumsw(enum s6_8_E e) {
    int r = -1;
    switch (e) {
    case s6_8_A: r = 0; break;
    case s6_8_B: r = 5; break;
    case s6_8_C: r = 6; break;
    }
    return r;
}

static int s6_8_jumpover(void) {
    int v = 0;
    switch (2) {
    int w = 111;
    case 2:
        w = 5;
        v = w;
        break;
    }
    return v;
}

static int s6_8_labelskip(void) {
    int r = 0;
    goto s6_8_lab;
    if (0) { s6_8_lab: r = 5; } else { r = 9; }
    return r;
}

static int s6_8_cont(void) {
    int c = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) { if (j == 1) continue; c++; }
    return c;
}

static int s6_8_brk(void) {
    int c = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 5; j++) { if (j == 2) break; c++; }
    return c;
}

static int s6_8_vlaprobe(void) {
    int n = 1, count = 0;
s6_8_again:
    ;
    int arr[n];
    arr[n - 1] = 7;
    count++;
    if (n < 3) { n++; goto s6_8_again; }
    return count;
}

void s6_8_statements(void) {
    /* 6.8.2p1 intermixed declarations and statements */
    int p = 2; p += 3; int q = p * 2;
    printf("mix=%d\n", q);

    /* 6.8.3p6 null-body while */
    const char *s = "abc"; int len = 0; while (s[len]) len++;
    printf("len=%d\n", len);

    /* 6.8.4.1p3 dangling else binds to nearest if */
    printf("dang=%d\n", s6_8_dang(1, 0));

    /* 6.8.4.1p2 first substatement reached via label; else not executed */
    printf("labelskip=%d\n", s6_8_labelskip());

    /* 6.8.4.2p4/p5 switch fallthrough + default */
    printf("sw=%d %d %d\n", s6_8_sw(1), s6_8_sw(2), s6_8_sw(9));

    /* 6.8.4.2p5 no match, no default: body not executed */
    printf("nomatch=%d\n", s6_8_nomatch());

    /* 6.8.4.2 enum-typed switch controlling expression */
    printf("enumsw=%d %d %d\n", s6_8_enumsw(s6_8_A), s6_8_enumsw(s6_8_B), s6_8_enumsw(s6_8_C));

    /* 6.8.4.2p7 object decl jumped over is reachable (initializer not run) */
    printf("jumpover=%d\n", s6_8_jumpover());

    /* 6.8.5.3p1 multi-declarator for scope + count */
    int iters = 0;
    for (int i = 0, j = 10; i < j; i++, j--) iters++;
    printf("iters=%d\n", iters);

    /* 6.8.5.2p1 do-while executes body once even with false condition */
    int dc = 0; do { dc++; } while (0);
    printf("do=%d\n", dc);

    /* 6.8.5.3p2 for(;;) with break */
    int inf = 0; for (;;) { inf++; if (inf == 7) break; }
    printf("inf=%d\n", inf);

    /* 6.8.6.2p2 nested continue affects only inner loop */
    printf("cont=%d\n", s6_8_cont());

    /* 6.8.6.3p2 nested break terminates only inner loop */
    printf("brk=%d\n", s6_8_brk());

    /* 6.8p3 / 6.8.6.1p4 backward goto re-creates block-scope VLA */
    printf("vla=%d\n", s6_8_vlaprobe());

    /* 6.8.6.4p3 return value converted as if by assignment */
    printf("retd=%.1f retc=%u\n", s6_8_retd(), (unsigned)s6_8_retc());

    /* 6.8.6.4p4 struct return may alias the caller's argument */
    s6_8_S x = { 1, 2, 3 };
    x = s6_8_id(x);
    printf("alias=%d%d%d\n", x.a, x.b, x.c);
}
