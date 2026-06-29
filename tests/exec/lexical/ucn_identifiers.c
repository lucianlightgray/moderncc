/* 6.4.2.1/6.4.3: universal character names are permitted as identifier
   characters, at the start of and within an identifier. A UCN names the same
   identifier as the equivalent literal UTF-8 spelling. Here each name is
   DECLARED with the \u/\U escape and USED via the literal UTF-8 form, proving
   they denote the same object. */
extern int printf(const char *, ...);
int \u00e9 = 10;
int caf\u00e9 = 20;
int wide_\U000001ff = 30;
int main(void)
{
    int ok = 1;
    é += 1; ok &= (é == 11);
    café += 5; ok &= (café == 25);
    ok &= (wide_ǿ == 30);
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
