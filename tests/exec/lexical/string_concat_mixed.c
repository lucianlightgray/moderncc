/* 6.4.5p5: a narrow string literal adjacent to a wide one is widened to the wide
   element type. Verifies wide-first + narrow concatenation (was miscompiled:
   the narrow bytes were copied raw into the wide array) plus same-prefix and
   narrow chains. */
extern int printf(const char *, ...);

typedef __WCHAR_TYPE__ wchar_t;

static int wlen(const wchar_t *s) { int n = 0; while (s[n]) n++; return n; }
static int slen(const char *s)    { int n = 0; while (s[n]) n++; return n; }

int main(void)
{
    int ok = 1;
    const wchar_t *s  = L"ab" "cd";        /* wide first, narrow widened  */
    const wchar_t *s2 = L"xy" L"zw";       /* same wide prefix            */
    const char    *n  = "ab" "cd" "ef";    /* narrow chain                */
    wchar_t w[] = L"pq" "rs";              /* wide array, narrow widened  */
    /* 6.4.5p5: NARROW-FIRST runs widen to the run's widest prefix too. */
    const wchar_t *nf  = "ab" L"cd";       /* narrow-first expr           */
    const wchar_t *nf2 = "x" "y" L"z";     /* narrow-narrow-wide expr     */
    wchar_t wnf[] = "x" L"y";              /* narrow-first declared array */
    wchar_t wnf3[3] = "p" L"q";            /* narrow-first explicitly-sized   */

    if (!(s[0]==(wchar_t)'a' && s[1]==(wchar_t)'b' && s[2]==(wchar_t)'c'
          && s[3]==(wchar_t)'d' && s[4]==0 && wlen(s)==4)) ok = 0;
    if (wlen(s2) != 4) ok = 0;
    if (slen(n) != 6) ok = 0;
    if (!(w[3]==(wchar_t)'s' && wlen(w)==4)) ok = 0;
    if (!(nf[0]==(wchar_t)'a' && nf[3]==(wchar_t)'d' && wlen(nf)==4)) ok = 0;
    if (!(nf2[0]==(wchar_t)'x' && nf2[2]==(wchar_t)'z' && wlen(nf2)==3)) ok = 0;
    if (!(wnf[0]==(wchar_t)'x' && wnf[1]==(wchar_t)'y' && wlen(wnf)==2)) ok = 0;
    if (!(wnf3[0]==(wchar_t)'p' && wnf3[1]==(wchar_t)'q' && wnf3[2]==0)) ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
