/* C9911 §7.22 <stdlib.h> (s7_22) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_22.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
static int s7_22_cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

void s7_22_strtol_test(void)
{
    char *end;
    /* base 10, leading whitespace + sign, endptr on trailing text */
    long v = strtol("   -123abc", &end, 10);
    printf("dec %ld end=%s\n", v, end);
    /* base 0: decimal / octal / hex auto-detect */
    printf("auto %ld %ld %ld\n",
           strtol("42", 0, 0), strtol("0755", 0, 0), strtol("0x2A", 0, 0));
    /* base 16 with optional 0x prefix */
    printf("hex %ld\n", strtol("0xFF", 0, 16));
    /* base 2 and base 36 letter values */
    printf("bin %ld base36 %ld\n",
           strtol("1011", 0, 2), strtol("Zz", 0, 36));
    /* no conversion possible -> 0, endptr == start */
    v = strtol("xyz", &end, 10);
    printf("noconv %ld same=%d\n", v, end == (char *)0 ? -1 : 1);
    /* unsigned + negation wrap in return type */
    printf("uneg %lu\n", strtoul("-1", 0, 10));
    /* overflow clamp to LONG_MAX / ULONG_MAX (glibc, host long width) */
    printf("clamp %d %d\n",
           strtol("999999999999999999999999", 0, 10)
               == strtol("9223372036854775807", 0, 10) ? 1 : 0,
           strtoul("999999999999999999999999", 0, 10)
               == strtoul("18446744073709551615", 0, 10) ? 1 : 0);
    /* long long / unsigned long long variants */
    printf("ll %lld ull %llu\n",
           strtoll("-77", 0, 10), strtoull("300", 0, 10));
}

void s7_22_intarith_test(void)
{
    /* atoi / atol / atoll */
    printf("ato %d %ld %lld\n", atoi("  -8x"), atol("100"), atoll("-9000000000"));
    /* abs / labs / llabs */
    printf("abs %d %ld %lld\n", abs(-5), labs(-123456L), llabs(-9000000000LL));
    printf("absp %d %ld %lld\n", abs(7), labs(7L), llabs(7LL));
    /* div / ldiv / lldiv: truncation toward zero, sign of remainder */
    div_t d = div(-7, 2);
    ldiv_t ld = ldiv(7L, -2L);
    lldiv_t lld = lldiv(-9000000001LL, 3LL);
    printf("div %d %d\n", d.quot, d.rem);
    printf("ldiv %ld %ld\n", ld.quot, ld.rem);
    printf("lldiv %lld %lld\n", lld.quot, lld.rem);
    /* identity: quot*denom + rem == numer */
    printf("ident %d\n", d.quot * 2 + d.rem == -7);
}

void s7_22_sortsearch_test(void)
{
    int a[7] = { 5, 3, 9, 1, 7, 3, 8 };
    qsort(a, 7, sizeof(int), s7_22_cmp_int);
    printf("sorted");
    for (int i = 0; i < 7; i++) printf(" %d", a[i]);
    printf("\n");
    int key = 7;
    int *hit = (int *)bsearch(&key, a, 7, sizeof(int), s7_22_cmp_int);
    printf("found7 %d\n", hit && *hit == 7);
    key = 4;
    hit = (int *)bsearch(&key, a, 7, sizeof(int), s7_22_cmp_int);
    printf("miss4 %d\n", hit == (int *)0);
}

void s7_22_mem_test(void)
{
    /* calloc zeroes the whole block */
    int *c = (int *)calloc(8, sizeof(int));
    long sum = 0;
    for (int i = 0; i < 8; i++) sum += c[i];
    printf("callocsum %ld\n", sum);
    /* realloc preserves contents up to the lesser size */
    int *p = (int *)malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) p[i] = i * 10 + 1;
    p = (int *)realloc(p, 16 * sizeof(int));
    printf("realloc %d %d %d %d\n", p[0], p[1], p[2], p[3]);
    free(c);
    free(p);
    /* realloc(NULL, n) behaves like malloc */
    int *q = (int *)realloc((void *)0, 3 * sizeof(int));
    q[0] = 42;
    printf("reallocnull %d\n", q[0]);
    free(q);
}
