/* C9911 §6.10.1-§6.10.3 Conditional inclusion/macros (s6_10_1) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_10_1.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
#define S6_10_1_STR2(x) #x
#define S6_10_1_STR(x) S6_10_1_STR2(x)
#define S6_10_1_CAT(a,b) a ## b
#define S6_10_1_XCAT(a,b) S6_10_1_CAT(a,b)

#define S6_10_1_ALIAS_UNDEF s6_10_1_still_undef
#define S6_10_1_ALIAS_ONE   s6_10_1_ONEVAL
#define s6_10_1_ONEVAL 1

#define S6_10_1_AF() S6_10_1_BF
#define S6_10_1_BF() 42

void s6_10_1_preproc_test(void)
{
    /* §6.10.1p4/p1: character constants evaluated as integers */
#if 'A' == 65
    printf("cc_A65=yes\n");
#else
    printf("cc_A65=no\n");
#endif
#if 'A' < 'B'
    printf("cc_AltB=yes\n");
#endif
#if '\n' == 10
    printf("cc_nl=yes\n");
#endif

    /* §6.10.1p4: greatest-width unsigned arithmetic */
#if 0xFFFFFFFFFFFFFFFFULL > 0
    printf("cc_bigu=yes\n");
#endif
#if (-1) < 0
    printf("cc_neg=yes\n");
#endif

    /* §6.10.1p4: identifier alias reduces to final residual then to 0 */
#if S6_10_1_ALIAS_UNDEF
    printf("alias_undef=nonzero\n");
#else
    printf("alias_undef=zero\n");
#endif
#if S6_10_1_ALIAS_ONE
    printf("alias_one=nonzero\n");
#else
    printf("alias_one=zero\n");
#endif

    /* §6.10.1p1: defined operator, both forms, via alias name */
#if defined S6_10_1_ALIAS_ONE && defined(s6_10_1_ONEVAL)
    printf("defined_both=yes\n");
#endif
#if !defined s6_10_1_never_defined_xyz
    printf("defined_neg=yes\n");
#endif

    /* §6.10.3.3p2/p3: placemarker with ## */
    {
        int foovar = 7;
        printf("pm_lead=%d\n", S6_10_1_CAT(,foovar));
        printf("pm_trail=%d\n", S6_10_1_CAT(foovar,));
    }

    /* §6.10.3.2p2: stringize whitespace collapse + spelling + escaping */
    printf("sz_ws=[%s]\n", S6_10_1_STR2(hello   world));
    printf("sz_lt=[%s]\n", S6_10_1_STR2(   x   y   ));
    printf("sz_esc=[%s]\n", S6_10_1_STR2("a\b"));

    /* §6.10.3.4p1: rescan consumes following source tokens */
    printf("consume=%d\n", S6_10_1_AF()());
}
