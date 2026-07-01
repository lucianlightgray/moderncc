/* C9911 §7.25-§7.27 threads(C11)/time (s7_25) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_25.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
/* §7.27.3.1p2 asctime: standard-fixed, locale-independent output format */
void s7_25_asctime(void)
{
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_wday = 3;   /* Wed */
    t.tm_mon  = 0;   /* Jan */
    t.tm_mday = 1;
    t.tm_hour = 13;
    t.tm_min  = 2;
    t.tm_sec  = 3;
    t.tm_year = 97;  /* 1997 */
    printf("asctime=[%s]", asctime(&t));
}

/* §7.27.3.3 gmtime + §7.27.3.5 strftime numeric/fixed specifiers + p6 return value */
void s7_25_strftime(void)
{
    time_t t = 0;                 /* 1970-01-01 00:00:00 UTC, Thursday */
    struct tm *g = gmtime(&t);
    char buf[256];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", g);
    printf("s1=%s\n", buf);
    strftime(buf, sizeof buf, "%F|%T|%D|%R", g);
    printf("s2=%s\n", buf);
    strftime(buf, sizeof buf, "j=%j u=%u w=%w C=%C y=%y e=%e", g);
    printf("s3=%s\n", buf);
    strftime(buf, sizeof buf, "lit%%%n%tend", g);
    printf("s4=[%s]\n", buf);
    /* §7.27.3.5p6: return value = char count (excl NUL) */
    printf("cnt=%zu\n", strftime(buf, sizeof buf, "%Y", g));
    /* §7.27.3.5p6: overflow -> returns 0 */
    char small[4];
    printf("ovf=%zu\n", strftime(small, sizeof small, "%Y", g));
}

/* §7.27.2.3p2/p3 mktime normalizes out-of-range fields (TZ-independent date math) */
void s7_25_mktime_norm(void)
{
    struct tm t;
    memset(&t, 0, sizeof t);
    t.tm_year = 100;   /* 2000 */
    t.tm_mon  = 13;    /* 13 months past Jan 2000 -> Feb 2001 */
    t.tm_mday = 1;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    mktime(&t);
    printf("norm year=%d mon=%d mday=%d\n", t.tm_year, t.tm_mon, t.tm_mday);
}

/* §7.27.2.2p2/p3 difftime (invariant, not raw value); §7.27.1p2 CLOCKS_PER_SEC/clock */
void s7_25_difftime(void)
{
    printf("diff=%d\n", difftime((time_t)100, (time_t)40) == 60.0);
    printf("diffneg=%d\n", difftime((time_t)40, (time_t)100) == -60.0);
    printf("cps_pos=%d\n", CLOCKS_PER_SEC > 0);
    printf("clock_ok=%d\n", clock() != (clock_t)-1);
}
