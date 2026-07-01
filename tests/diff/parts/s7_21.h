/* C9911 §7.21 <stdio.h> (s7_21) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_21.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
static int s7_21_vwrap(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}
static int s7_21_vscanwrap(const char *s, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}
static int s7_21_vpwrap(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

void s7_21_printf_flags(void)
{
    char b[64];
    int n;
    n = sprintf(b, "[%-5d]", 42);        printf("A %s len=%d\n", b, n);
    sprintf(b, "[%+d][% d]", 3, 3);      printf("B %s\n", b);
    sprintf(b, "[%#x][%#o]", 255, 8);    printf("C %s\n", b);
    sprintf(b, "[%05d]", 42);            printf("D %s\n", b);
    sprintf(b, "[%.3s]", "hello");       printf("E %s\n", b);
    sprintf(b, "[%*d][%.*f]", 4, 7, 2, 3.14159);  printf("F %s\n", b);
    sprintf(b, "[%%]");                  printf("G %s\n", b);
    sprintf(b, "[%.0d][%.0d]", 0, 5);    printf("H %s\n", b);
    n = sprintf(b, "%d and %d", -8, 9);  printf("I %s ret=%d\n", b, n);
}

void s7_21_length_mods(void)
{
    char b[128];
    signed char sc = -3;
    short sh = -4;
    long lo = 6L;
    long long ll = 7LL;
    uintmax_t j = 8;
    size_t z = 9;
    ptrdiff_t t = 10;
    sprintf(b, "%hhd %hd %ld %lld %ju %zu %td",
            (int)sc, (int)sh, lo, ll, j, z, t);
    printf("LM %s\n", b);
    unsigned u = 0xABCD;
    sprintf(b, "%x %X %o %u", u, u, 64u, 100u);
    printf("LU %s\n", b);
}

void s7_21_floatconv(void)
{
    char b[64];
    sprintf(b, "%e", 12345.678);   printf("FE %s\n", b);
    sprintf(b, "%E", 0.00012345);  printf("FF %s\n", b);
    sprintf(b, "%g %g", 123456.0, 0.0001);  printf("FG %s\n", b);
    sprintf(b, "%a", 1.5);         printf("FA %s\n", b);
    sprintf(b, "%A", 1.5);         printf("FB %s\n", b);
    sprintf(b, "%.2f", 2.5);       printf("FR %s\n", b);
}

void s7_21_snprintf_ret(void)
{
    char b[5];
    int r = snprintf(b, sizeof b, "%s", "abcdefgh");
    printf("SN buf='%s' ret=%d\n", b, r);
    int r0 = snprintf(NULL, 0, "%d", 12345);
    printf("SN0 ret=%d\n", r0);
    char b2[16];
    int r2 = snprintf(b2, sizeof b2, "x=%d", 42);
    printf("SN2 buf='%s' ret=%d\n", b2, r2);
}

void s7_21_sscanf_read(void)
{
    int a; double d; unsigned x; char word[16]; int consumed;
    int got = sscanf("123 45.6 ff", "%d %lf %x", &a, &d, &x);
    printf("SC got=%d a=%d d=%.1f x=%u\n", got, a, d, x);
    int g2 = sscanf("hello world", "%15s%n", word, &consumed);
    printf("SC2 g=%d w=%s n=%d\n", g2, word, consumed);
    char set[16];
    int g3 = sscanf("abc123", "%[a-z]", set);
    printf("SC3 g=%d s=%s\n", g3, set);
    int y = 0;
    int g4 = sscanf("nope", "%d", &y);
    printf("SC4 g=%d y=%d\n", g4, y);
}

void s7_21_vfamily(void)
{
    char b[32];
    int r = s7_21_vwrap(b, sizeof b, "n=%d s=%s", 5, "hi");
    printf("VS buf='%s' ret=%d\n", b, r);
    int v = 0;
    int g = s7_21_vscanwrap("77", "%d", &v);
    printf("VSC g=%d v=%d\n", g, v);
    int pr = s7_21_vpwrap("VP %d %s\n", 99, "ok");
    printf("VPR ret=%d\n", pr);
}
