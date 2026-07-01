/* C9911 §6.5.5-§6.5.14 Binary operators (s6_5_5) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_5_5.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
int s6_5_5_side_count;
static int s6_5_5_side(int r){ s6_5_5_side_count++; return r; }

void s6_5_5_binary_ops(void)
{
    /* §6.5.5p1/§6.5.6p1/§6.5.7p1/§6.5.8p1/§6.5.10-12p1: precedence chain */
    printf("prec %d %d %d %d %d\n",
        2+3*4, 1<<2+1, (1<2)==1, 5&3|4, 1|2^3&4);
    /* left-associativity of - and / (§6.5.5p1/§6.5.6p1) */
    printf("assoc %d %d\n", 10-3-2, 100/5/2);
    /* §6.5.5p5/p6: truncation toward zero + remainder identity */
    printf("div %d %d %d %d\n", -7/2, 7/-2, -7%2, 7%-2);
    int ok = 1;
    for (int a=-9; a<=9; a++)
        for (int b=-9; b<=9; b++)
            if (b) { if ((a/b)*b + a%b != a) ok=0; }
    printf("ident %d\n", ok);
    /* §6.5.5p4: product */
    printf("mul %d\n", 6*7);
    /* §6.5.6p7/p8: pointer + integer, one-past, and pointer subtraction */
    int arr[6] = {10,20,30,40,50,60};
    int *p = arr;
    printf("padd %d %d\n", *(p+3), *(3+p));
    printf("psub %d\n", (int)(&arr[5]-&arr[1]));
    printf("pend %d\n", (arr+6) == &arr[6]);
    ptrdiff_t dd = &arr[4] - arr;
    printf("pdiff %ld\n", (long)dd);
    /* §6.5.7p4/p5: shifts (unsigned modulo, arithmetic right shift) */
    printf("shl %u\n", 1u<<31);
    printf("shr %d %d\n", 240>>3, -16>>2);
    /* §6.5.8p6: relational operators yield int 0/1 */
    printf("rel %d %d %d %d\n", 3<5, 5<3, 5<=5, 7>=8);
    /* §6.5.9p3/p5: equality, null pointer constant, void* self-compare */
    int *q = arr;
    printf("eq %d %d %d %d\n", 4==4, 4!=4, (q==0), (q==(void*)q));
    /* §6.5.10p4/§6.5.11p4/§6.5.12p4: bitwise AND/XOR/OR */
    printf("bit %d %d %d\n", 0xF0&0x3C, 0xF0^0x3C, 0xF0|0x3C);
    /* §6.5.13p3/§6.5.14p3: logical AND/OR yield int 0/1 */
    printf("log %d %d %d %d\n", 3&&4, 0&&5, 0||0, 2||0);
    /* §6.5.13p4/§6.5.14p4: short-circuit evaluation */
    s6_5_5_side_count = 0;
    int r1 = (0 && s6_5_5_side(1));
    int r2 = (1 || s6_5_5_side(1));
    int r3 = (1 && s6_5_5_side(1));
    int r4 = (0 || s6_5_5_side(1));
    printf("sc %d %d %d %d cnt=%d\n", r1, r2, r3, r4, s6_5_5_side_count);
}
