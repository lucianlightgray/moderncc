/* Self-checking conformance for Apple's *genuine* libc string routines,
   compiled from Apple's verbatim open-source sources (see PROVENANCE.md) by
   mcc targeting x86_64 Mach-O, then loaded and executed as a Mach-O image by
   tests/qemu/macho/loader.c on a Linux host.

   This is the closest achievable approximation, off-macOS, to "Mach-O tested
   against its real target libc": the functions exercised here (strcspn,
   strpbrk, strsep, memmem, strchrnul, strnstr) are the actual code that ships
   in Apple's libsystem_c -- not a hand-written stand-in. The core hot-path
   routines (strlen/strcmp/memcpy/memset/...) ship in libSystem as Darwin
   commpage-dependent x86_64 assembly, and malloc/stdio are fused to Mach VM
   and the Darwin FILE/syscall stack, so those genuinely require a macOS or
   darling host; that boundary is documented in PROVENANCE.md.

   Exits 0 on success; a nonzero return identifies the failing check. */
#include <string.h>

int main(void)
{
    /* strcspn: length of initial span with NO chars from the set */
    if (strcspn("hello, world", ",") != 5) return 1;
    if (strcspn("abcdef", "xyz") != 6)     return 2;
    if (strcspn("", "abc") != 0)           return 3;

    /* strpbrk: first occurrence of any char from the set */
    {
        const char *s = "find the=sign";
        char *p = strpbrk(s, "=:");
        if (!p || *p != '=' || (p - s) != 8) return 4;
        if (strpbrk(s, "ZQ") != NULL)        return 5;
    }

    /* strsep: tokenize in place across an empty field */
    {
        char buf[] = "a,,b";
        char *sp = buf, *t;
        t = strsep(&sp, ",");  if (!t || strcspn(t, "") != 1 || t[0] != 'a') return 6;
        t = strsep(&sp, ",");  if (!t || t[0] != 0)                          return 7; /* empty field */
        t = strsep(&sp, ",");  if (!t || t[0] != 'b' || t[1] != 0)           return 8;
        t = strsep(&sp, ",");  if (t != NULL)                                return 9; /* exhausted */
    }

    /* memmem: locate a byte substring */
    {
        const char hay[] = "the quick brown fox";
        char *m = memmem(hay, sizeof(hay) - 1, "brown", 5);
        if (!m || (m - hay) != 10)                       return 10;
        if (memmem(hay, sizeof(hay) - 1, "cat", 3))      return 11;
        if (memmem(hay, sizeof(hay) - 1, "", 0) != NULL) return 12; /* Apple: empty needle -> NULL */
        if (memmem(hay, 0, "x", 1) != NULL)              return 18; /* Apple: empty haystack -> NULL */
    }

    /* strchrnul: like strchr but returns the terminator (not NULL) on miss */
    {
        const char *s = "path/to/file";
        char *slash = strchrnul(s, '/');
        if (!slash || *slash != '/' || (slash - s) != 4) return 13;
        char *none = strchrnul(s, 'Z');
        if (!none || *none != 0 || (none - s) != 12)     return 14; /* points at NUL */
    }

    /* strnstr: bounded substring search */
    {
        const char *s = "alpha beta gamma";
        if (strnstr(s, "beta", 16) != s + 6) return 15;
        if (strnstr(s, "gamma", 8) != NULL)  return 16; /* out of bound */
        if (strnstr(s, "", 16) != s)         return 17; /* empty needle */
    }

    return 0;
}
