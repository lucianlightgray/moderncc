/* C9911 §7.6-§7.8 fenv, float, inttypes (s7_6) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_6.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
int sscanf(const char *str, const char *format, ...);

/* C9911 section 7.7 float.h and 5.2.4.2.2 characteristics of floating types */
void s7_6_float_test(void)
{
    printf("s7_6 radix=%d\n", FLT_RADIX);
    printf("s7_6 radix_ge2=%d\n", FLT_RADIX >= 2);
    printf("s7_6 mant=%d\n", FLT_MANT_DIG > 0 && DBL_MANT_DIG >= FLT_MANT_DIG && LDBL_MANT_DIG >= DBL_MANT_DIG);
    printf("s7_6 dig=%d\n", FLT_DIG >= 6 && DBL_DIG >= 10 && LDBL_DIG >= 10);
    printf("s7_6 decdig=%d\n", FLT_DECIMAL_DIG >= 6 && DBL_DECIMAL_DIG >= 10 && LDBL_DECIMAL_DIG >= 10);
    printf("s7_6 decimaldig=%d\n", DECIMAL_DIG >= 10);
    printf("s7_6 minexp=%d\n", FLT_MIN_10_EXP <= -37 && DBL_MIN_10_EXP <= -37 && LDBL_MIN_10_EXP <= -37);
    printf("s7_6 maxexp=%d\n", FLT_MAX_10_EXP >= 37 && DBL_MAX_10_EXP >= 37 && LDBL_MAX_10_EXP >= 37);
    printf("s7_6 max=%d\n", FLT_MAX >= 1e37F && DBL_MAX >= 1e37 && LDBL_MAX >= 1e37L);
    printf("s7_6 eps=%d\n", FLT_EPSILON <= 1e-5F && DBL_EPSILON <= 1e-9 && LDBL_EPSILON <= 1e-9L);
    printf("s7_6 min=%d\n", FLT_MIN <= 1e-37F && DBL_MIN <= 1e-37 && LDBL_MIN <= 1e-37L);
    printf("s7_6 truemin=%d\n", FLT_TRUE_MIN <= 1e-37F && DBL_TRUE_MIN <= 1e-37 && LDBL_TRUE_MIN <= 1e-37L);
    printf("s7_6 subnorm=%d\n", FLT_HAS_SUBNORM == 1 && DBL_HAS_SUBNORM == 1 && LDBL_HAS_SUBNORM == 1);
    printf("s7_6 evalmethod=%d\n", FLT_EVAL_METHOD == 0);
    printf("s7_6 minlt1=%d\n", FLT_MIN < 1.0F && FLT_MIN > 0.0F);
    printf("s7_6 maxfin=%d\n", DBL_MAX > 0.0 && (DBL_MAX * 2.0) > DBL_MAX);
    {
        volatile double one = 1.0;
        volatile double s = one + DBL_EPSILON;
        volatile double h = one + DBL_EPSILON / 2.0;
        printf("s7_6 eps_gap=%d\n", s != one && h == one);
    }
#if FLT_RADIX < 2
#error radix
#endif
#if FLT_MANT_DIG < 1 || DBL_MANT_DIG < FLT_MANT_DIG
#error mant
#endif
#if FLT_MIN_10_EXP > -37 || DBL_MAX_10_EXP < 37 || DECIMAL_DIG < 10 || FLT_HAS_SUBNORM != 1
#error ppconst
#endif
    printf("s7_6 ppconst=%d\n", 1);
}

/* C9911 section 7.8 inttypes.h: PRI and SCN format macros + imax/strtoNmax functions */
void s7_6_inttypes_test(void)
{
    printf("s7_6 imaxabs=%d\n", (int)(imaxabs((intmax_t)-42) == 42));
    imaxdiv_t dv = imaxdiv((intmax_t)17, (intmax_t)5);
    printf("s7_6 imaxdiv=%d\n", (int)(dv.quot == 3 && dv.rem == 2));
    imaxdiv_t dv2 = imaxdiv((intmax_t)-17, (intmax_t)5);
    printf("s7_6 imaxdivneg=%d\n", (int)(dv2.quot == -3 && dv2.rem == -2));

    char *end;
    intmax_t si = strtoimax("  -ff", &end, 16);
    printf("s7_6 strtoimax=%d\n", (int)(si == -255 && *end == 0));
    uintmax_t ui = strtoumax("100", (char**)0, 10);
    printf("s7_6 strtoumax=%d\n", (int)(ui == 100));

    printf("s7_6 stdint=%d\n", (int)(INT32_MAX == 2147483647));

    char buf[80];
    sprintf(buf, "%" PRIdMAX, (intmax_t)-1234567890);
    printf("s7_6 pridmax=%s\n", buf);
    sprintf(buf, "%" PRIiMAX, (intmax_t)-77);
    printf("s7_6 priimax=%s\n", buf);
    sprintf(buf, "%" PRId32 " %" PRIu64, (int32_t)-7, (uint64_t)18446744073709551615ULL);
    printf("s7_6 prid=%s\n", buf);
    sprintf(buf, "%" PRIx16 " %" PRIoFAST8 " %" PRIXMAX, (uint16_t)0xABCD, (uint_fast8_t)9, (uintmax_t)0xDEAD);
    printf("s7_6 prix=%s\n", buf);
    sprintf(buf, "%" PRIuLEAST16, (uint_least16_t)65535);
    printf("s7_6 prileast=%s\n", buf);
    sprintf(buf, "%020" PRIdMAX, (intmax_t)1234567890123LL);
    printf("s7_6 pri020=%s\n", buf);

    int64_t v64 = 0;
    sscanf("-98765", "%" SCNd64, &v64);
    printf("s7_6 scnd64=%d\n", (int)(v64 == -98765));
    uint32_t v32 = 0;
    sscanf("abcd", "%" SCNx32, &v32);
    printf("s7_6 scnx32=%d\n", (int)(v32 == 0xabcd));
    uint_least16_t vl16 = 0;
    sscanf("255", "%" SCNuLEAST16, &vl16);
    printf("s7_6 scnul16=%d\n", (int)(vl16 == 255));
    intmax_t vmax = 0;
    sscanf("-1234567890123", "%" SCNdMAX, &vmax);
    printf("s7_6 scndmax=%d\n", (int)(vmax == -1234567890123LL));
    uint32_t vo = 0;
    sscanf("17", "%" SCNo32, &vo);
    printf("s7_6 scno32=%d\n", (int)(vo == 017));
}
