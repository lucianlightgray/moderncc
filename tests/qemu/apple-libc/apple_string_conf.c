#include <string.h>

int main(void) {

    if (strcspn("hello, world", ",") != 5)
        return 1;
    if (strcspn("abcdef", "xyz") != 6)
        return 2;
    if (strcspn("", "abc") != 0)
        return 3;

    {
        const char *s = "find the=sign";
        char *p = strpbrk(s, "=:");
        if (!p || *p != '=' || (p - s) != 8)
            return 4;
        if (strpbrk(s, "ZQ") != NULL)
            return 5;
    }

    {
        char buf[] = "a,,b";
        char *sp = buf, *t;
        t = strsep(&sp, ",");
        if (!t || strcspn(t, "") != 1 || t[0] != 'a')
            return 6;
        t = strsep(&sp, ",");
        if (!t || t[0] != 0)
            return 7;
        t = strsep(&sp, ",");
        if (!t || t[0] != 'b' || t[1] != 0)
            return 8;
        t = strsep(&sp, ",");
        if (t != NULL)
            return 9;
    }

    {
        const char hay[] = "the quick brown fox";
        char *m = memmem(hay, sizeof(hay) - 1, "brown", 5);
        if (!m || (m - hay) != 10)
            return 10;
        if (memmem(hay, sizeof(hay) - 1, "cat", 3))
            return 11;
        if (memmem(hay, sizeof(hay) - 1, "", 0) != NULL)
            return 12;
        if (memmem(hay, 0, "x", 1) != NULL)
            return 18;
    }

    {
        const char *s = "path/to/file";
        char *slash = strchrnul(s, '/');
        if (!slash || *slash != '/' || (slash - s) != 4)
            return 13;
        char *none = strchrnul(s, 'Z');
        if (!none || *none != 0 || (none - s) != 12)
            return 14;
    }

    {
        const char *s = "alpha beta gamma";
        if (strnstr(s, "beta", 16) != s + 6)
            return 15;
        if (strnstr(s, "gamma", 8) != NULL)
            return 16;
        if (strnstr(s, "", 16) != s)
            return 17;
    }

    return 0;
}
