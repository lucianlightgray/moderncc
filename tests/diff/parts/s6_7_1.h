/* C9911 §6.7.1-§6.7.3 Declarations: storage/type/qualifiers (s6_7_1) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_7_1.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
struct s6_7_1_agg { int a; long b; char c; };
static struct s6_7_1_agg s6_7_1_static_agg;

void s6_7_1_storage_qual_test(void)
{
    /* §6.7.1p8: the storage-class specifier of a static aggregate applies
       recursively to its members -> static storage duration -> members are
       zero-initialized. */
    printf("agg=%d %ld %d\n", s6_7_1_static_agg.a, s6_7_1_static_agg.b,
           (int)s6_7_1_static_agg.c);

    /* §6.7.3p5: if the same qualifier appears more than once in a
       specifier-qualifier-list it behaves as if it appeared only once. */
    const const int s6_7_1_cc = 7;
    printf("cc=%d\n", s6_7_1_cc);

    /* §6.7.3p10: the order of type qualifiers does not affect the specified
       type; both spellings name the same qualified type. */
    const volatile int s6_7_1_cv = 3;
    volatile const int s6_7_1_vc = 3;
    printf("cvvc=%d\n", s6_7_1_cv == s6_7_1_vc);

    /* §6.7.2p2 multiset table: 'unsigned long long int' designates the same
       type as 'unsigned long long'; 'signed long long' == 'long long int'. */
    unsigned long long int s6_7_1_ulli = 0xFFFFFFFFFFFFFFFFULL;
    unsigned long long      s6_7_1_ull  = 0xFFFFFFFFFFFFFFFFULL;
    printf("ull=%d\n", s6_7_1_ulli == s6_7_1_ull);
    long long int  s6_7_1_lli = -5;
    signed long long s6_7_1_sll = -5;
    printf("lli=%d\n", s6_7_1_lli == s6_7_1_sll);
    printf("ullsz=%d llsz=%d\n",
           (int)(sizeof(unsigned long long int) == sizeof(unsigned long long)),
           (int)(sizeof(signed long long) == sizeof(long long int)));

    /* §6.7.2p2 multiset: 'signed' / 'signed int' / 'int' are all the same type. */
    signed s6_7_1_s = -1;
    signed int s6_7_1_si = -1;
    int s6_7_1_i = -1;
    printf("intset=%d\n", s6_7_1_s == s6_7_1_si && s6_7_1_si == s6_7_1_i);

    /* §6.7.2.4p1/p4: the '_Atomic ( type-name )' atomic-type-specifier form and
       the '_Atomic' qualifier spelling side-by-side; simple read/modify/write
       lowers to an atomic load then atomic store. */
    _Atomic(int) s6_7_1_at = 10;
    _Atomic int  s6_7_1_aq = 20;
    s6_7_1_at = s6_7_1_at + 5;
    s6_7_1_aq = s6_7_1_aq - 5;
    printf("atomic=%d %d\n", (int)s6_7_1_at, (int)s6_7_1_aq);

    /* §6.7.1p6: sizeof may be applied to a register-qualified array (the only
       operator permitted on it). */
    register int s6_7_1_ra[4];
    printf("rasz=%d\n", (int)(sizeof(s6_7_1_ra) == 4 * sizeof(int)));
}
