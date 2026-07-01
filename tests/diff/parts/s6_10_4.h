/* C9911 §6.10.4-§6.10.9 Line/error/pragma/predefined/_Pragma (s6_10_4) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_10_4.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
#define s6_10_4_LN 4000
#define s6_10_4_DOPRAGMA(x) _Pragma(#x)

void s6_10_4_preproc_test(void)
{
    /* §6.10.8.1: mandatory predefined macros (values invariant under -std=gnu11) */
    printf("__STDC__=%d\n", __STDC__);
    printf("__STDC_HOSTED__=%d\n", __STDC_HOSTED__);
    printf("__STDC_VERSION__=%ldL\n", __STDC_VERSION__);

    /* §6.10.8.1: __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss") have fixed shapes;
       content varies by build so probe only the invariant layout */
    printf("date_len=%d time_len=%d\n", (int)sizeof(__DATE__) - 1, (int)sizeof(__TIME__) - 1);
    printf("time_colons=%d %d\n", __TIME__[2] == ':', __TIME__[5] == ':');
    printf("date_sp=%d\n", __DATE__[3] == ' ');

    /* §6.10.8.1: __func__ names the enclosing function */
    printf("func=%s\n", __func__);

    /* §6.10.7p1: the null directive has no effect */
#
    printf("after_null=1\n");

    /* §6.10.4p2: #line sets the presumed line number of the immediately following line */
#line 3000
    printf("line_p2=%d\n", __LINE__);

    /* §6.10.4p3: another plain #line resets __LINE__ again */
#line 3100
    printf("line_p3a=%d\n", __LINE__);

    /* §6.10.4p4: the trailing #line tokens are macro-expanded before being matched */
#line s6_10_4_LN
    printf("line_p4=%d\n", __LINE__);

    /* §6.10.6p1 / §6.10.9p1: an unrecognized #pragma, an unknown _Pragma emitted via the
       DO_PRAGMA(#x) idiom, and a plain unknown _Pragma are all ignored; program runs */
#pragma s6_10_4_bogus_pragma_name 12 34
    s6_10_4_DOPRAGMA(s6_10_4_unknown_via_operator)
    _Pragma("s6_10_4_also_unknown")
    printf("pragma_ok=1\n");
}
