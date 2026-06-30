
extern int printf(const char *, ...);

int main(void)
{
    const char *s = u8"hello";
    char buf[] = u8"abc";
    const char *cat = u8"x" "y";
    int u8 = 42;
    unsigned u = 1;

    int ok = s[0] == 'h' && s[4] == 'o'
          && sizeof buf == 4 && buf[2] == 'c'
          && cat[0] == 'x' && cat[1] == 'y'
          && u8 == 42 && u == 1u;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
