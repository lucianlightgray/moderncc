/* C9911 §7.1-§7.5 Library intro, assert, complex, ctype, errno (s7_1) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_1.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
void s7_1_ctype(void)
{
    int cnt_alpha=0,cnt_digit=0,cnt_upper=0,cnt_lower=0,cnt_alnum=0,
        cnt_space=0,cnt_blank=0,cnt_xdig=0,cnt_punct=0,cnt_cntrl=0,
        cnt_graph=0,cnt_print=0;
    for (int c = 0; c < 128; c++) {
        if (isalpha(c)) cnt_alpha++;
        if (isdigit(c)) cnt_digit++;
        if (isupper(c)) cnt_upper++;
        if (islower(c)) cnt_lower++;
        if (isalnum(c)) cnt_alnum++;
        if (isspace(c)) cnt_space++;
        if (isblank(c)) cnt_blank++;
        if (isxdigit(c)) cnt_xdig++;
        if (ispunct(c)) cnt_punct++;
        if (iscntrl(c)) cnt_cntrl++;
        if (isgraph(c)) cnt_graph++;
        if (isprint(c)) cnt_print++;
    }
    printf("ctype counts %d %d %d %d %d %d %d %d %d %d %d %d\n",
           cnt_alpha,cnt_digit,cnt_upper,cnt_lower,cnt_alnum,cnt_space,
           cnt_blank,cnt_xdig,cnt_punct,cnt_cntrl,cnt_graph,cnt_print);
    /* §7.4.1.1: isalnum true iff isalpha||isdigit for every c */
    int consistent = 1;
    for (int c = 0; c < 128; c++)
        if ((isalnum(c)!=0) != (isalpha(c)!=0 || isdigit(c)!=0)) consistent = 0;
    printf("alnum=alpha|digit %d\n", consistent);
    /* §7.4.1.6/7.4.1.8: isgraph == isprint && !space; isprint==isgraph||' ' */
    int gp = 1;
    for (int c = 0; c < 128; c++)
        if ((isgraph(c)!=0) != (isprint(c)!=0 && c!=' ')) gp = 0;
    printf("graph=print&!sp %d\n", gp);
    /* §7.4.1.3 isblank: only space and tab true in the C locale */
    printf("blank tab=%d sp=%d a=%d\n",
           isblank('\t')!=0, isblank(' ')!=0, isblank('a')!=0);
    /* §7.4p1 argument EOF returns 0 */
    printf("EOF cls %d %d %d\n", isalpha(EOF)!=0, isspace(EOF)!=0, isdigit(EOF)!=0);
    /* §7.4.2 tolower/toupper roundtrip over letters, identity on non-letters */
    int rt = 1, ident = 1;
    for (int c = 'A'; c <= 'Z'; c++) if (toupper(tolower(c)) != c) rt = 0;
    for (int c = 'a'; c <= 'z'; c++) if (tolower(toupper(c)) != c) rt = 0;
    for (int c = '0'; c <= '9'; c++) if (toupper(c) != c || tolower(c) != c) ident = 0;
    printf("case rt=%d ident=%d A->%c a->%c\n", rt, ident, tolower('A'), toupper('a'));
    /* §7.1.4p1 macro suppression via parentheses calls the real function
       (return value is unspecified nonzero, so normalize) */
    printf("paren %d %d\n", (isalpha)('a')!=0, (isdigit)('5')!=0);
    /* §7.1.4p1 the name can be used to take the address of the real function */
    int (*q)(int) = isalpha;
    printf("fnptr %d %d\n", q('z')!=0, q('7')!=0);
}

void s7_1_complex(void)
{
    double complex z = 3.0 + 4.0*I;
    /* §7.3.9.6 creal, §7.3.9.2 cimag as real double parts */
    printf("re=%g im=%g\n", creal(z), cimag(z));
    /* §7.3.9.2p4 z == creal(z)+cimag(z)*I fully describes z */
    double complex z2 = creal(z) + cimag(z)*I;
    printf("recompose %d\n", creal(z2) == creal(z) && cimag(z2) == cimag(z));
    /* §7.3.9.4 conjugate reverses the sign of the imaginary part */
    double complex zc = creal(z) - cimag(z)*I;
    printf("conj re=%g im=%g\n", creal(zc), cimag(zc));
    /* §7.3.9.3 CMPLX builds a complex value from real and imaginary parts */
    double complex w = CMPLX(1.0, 2.0);
    printf("cmplx re=%g im=%g\n", creal(w), cimag(w));
    /* §7.3.9.3p3 CMPLX preserves the sign of a zero imaginary part */
    double complex nz = CMPLX(1.0, -0.0);
    printf("cmplx negzero signbit=%d\n", (int)(1.0/cimag(nz) < 0));
    /* §7.3.1p3/p5 I is the imaginary unit: I*I == -1 */
    double complex ii = I*I;
    printf("I*I re=%g im=%g\n", creal(ii), cimag(ii));
    /* §7.3.1p3 complex expands to _Complex; native arithmetic */
    complex double sum = z + w;
    complex double prod = z * w;
    printf("sum re=%g im=%g\n", creal(sum), cimag(sum));
    printf("prod re=%g im=%g\n", creal(prod), cimag(prod));
}

void s7_1_errno(void)
{
    /* §7.5p2 EDOM, ERANGE, EILSEQ: distinct positive int constant expressions */
    printf("errno macros pos=%d distinct=%d\n",
           (EDOM>0 && ERANGE>0 && EILSEQ>0),
           (EDOM!=ERANGE && ERANGE!=EILSEQ && EDOM!=EILSEQ));
    /* §7.5p2 usable in #if preprocessing directives */
#if EDOM > 0 && ERANGE > 0 && EILSEQ > 0
    printf("errno #if ok\n");
#endif
    /* §7.5p3 errno is a modifiable int lvalue */
    errno = 0;
    printf("errno0=%d\n", errno);
    errno = ERANGE;
    printf("errno set match=%d\n", errno == ERANGE);
    int *ep = &errno;
    *ep = EDOM;
    printf("errno via ptr match=%d\n", errno == EDOM);
    errno = 0;
    printf("errno reset=%d\n", errno);
}
