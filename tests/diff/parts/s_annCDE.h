/* C9911 Annexes C, D, E, H, I, J (s_annCDE) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s_annCDE.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
static int s_annCDE_ctr;
static int s_annCDE_bump(int r){ s_annCDE_ctr++; return r; }

void s_annCDE_seqpoint_test(void)
{
    int a, b, c, e, f, g, h, i;

    /* 6.5.13: sequence point between && operands; RHS suppressed when LHS==0 */
    s_annCDE_ctr = 0;
    a = (0 && s_annCDE_bump(1));
    printf("and0 v=%d calls=%d\n", a, s_annCDE_ctr);
    s_annCDE_ctr = 0;
    b = (1 && s_annCDE_bump(9));
    printf("and1 v=%d calls=%d\n", b, s_annCDE_ctr);

    /* 6.5.14: sequence point between || operands; RHS suppressed when LHS!=0 */
    s_annCDE_ctr = 0;
    c = (1 || s_annCDE_bump(1));
    printf("or1 v=%d calls=%d\n", c, s_annCDE_ctr);
    s_annCDE_ctr = 0;
    h = (0 || s_annCDE_bump(1));
    printf("or0 v=%d calls=%d\n", h, s_annCDE_ctr);

    /* 6.5.17: comma; LHS side effect committed before RHS is evaluated */
    s_annCDE_ctr = 0;
    e = (s_annCDE_bump(5), s_annCDE_ctr);
    printf("comma v=%d calls=%d\n", e, s_annCDE_ctr);

    /* 6.5.15: ?: evaluates only the selected arm */
    s_annCDE_ctr = 0;
    f = (1 ? s_annCDE_bump(10) : s_annCDE_bump(20));
    printf("cond1 v=%d calls=%d\n", f, s_annCDE_ctr);
    s_annCDE_ctr = 0;
    g = (0 ? s_annCDE_bump(10) : s_annCDE_bump(20));
    printf("cond0 v=%d calls=%d\n", g, s_annCDE_ctr);

    /* nested short-circuit chain: only calls up to the decisive operand run */
    s_annCDE_ctr = 0;
    i = (s_annCDE_bump(1) && s_annCDE_bump(0) && s_annCDE_bump(1));
    printf("chain v=%d calls=%d\n", i, s_annCDE_ctr);
}
