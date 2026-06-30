
















typedef unsigned long size_t;
int   _simple_snprintf(char *str, size_t size, const char *fmt, ...);
int   strcmp(const char *, const char *);

static int eq(const char *fmt_result, const char *expect)
{
    return strcmp(fmt_result, expect) == 0;
}

int main(void)
{
    char b[128];

    _simple_snprintf(b, sizeof b, "plain");
    if (!eq(b, "plain")) return 1;

    _simple_snprintf(b, sizeof b, "int=%d neg=%d", 42, -7);
    if (!eq(b, "int=42 neg=-7")) return 2;

    _simple_snprintf(b, sizeof b, "u=%u x=%x X=%X o=%o", 305419896u, 0xdeadbeefu, 0xCAFEu, 64u);
    if (!eq(b, "u=305419896 x=deadbeef X=CAFE o=100")) return 3;

    _simple_snprintf(b, sizeof b, "str=%s chr=%c", "hi", '!');
    if (!eq(b, "str=hi chr=!")) return 4;


    _simple_snprintf(b, sizeof b, "[%5d][%05d]", 42, 42);
    if (!eq(b, "[   42][00042]")) return 5;


    _simple_snprintf(b, sizeof b, "ll=%lld lx=%lx", 3000000000LL, 0x1ffffffffUL);
    if (!eq(b, "ll=3000000000 lx=1ffffffff")) return 6;


    _simple_snprintf(b, sizeof b, "p=%p", (void *)0x1234);
    if (!eq(b, "p=0x1234")) return 7;


    if (_simple_snprintf(b, sizeof b, "abcd") != 4) return 8;


    {
        char small[4];
        _simple_snprintf(small, sizeof small, "%d", 99999);
        if (small[3] != '\0') return 9;
    }

    return 0;
}
