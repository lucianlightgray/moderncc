




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
