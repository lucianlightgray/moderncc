/* Self-checking conformance for Apple's *genuine* core string/memory routines,
   compiled from Apple's verbatim libplatform sources (apple-oss-distributions/
   libplatform, src/string/generic/ -- see PROVENANCE.md) by mcc targeting
   x86_64 Mach-O, then loaded and executed as a Mach-O image by the in-repo
   loader on a Linux host.

   These are the very functions a previous note called "kernel-fused / commpage
   assembly, impossible off-macOS": strlen, strcmp, strncmp, strcpy, strncpy,
   strlcpy, strlcat, strchr, strstr, strnlen, memmove, memcpy, memcmp, memchr,
   memccpy, bzero, memset, memset_pattern4. The commpage assembly is only an
   *optimization variant*; Apple ships these portable C implementations as the
   functional equivalent (gated by _PLATFORM_OPTIMIZED_*), and THEY run fine as
   a Mach-O image off-Darwin. So this is Apple's real core libc, executed.

   Exits 0 on success; a nonzero return identifies the failing check. */

typedef unsigned long size_t;
#define NULL ((void *)0)
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
int    strcmp(const char *, const char *);
int    strncmp(const char *, const char *, size_t);
char  *strcpy(char *, const char *);
char  *strncpy(char *, const char *, size_t);
size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);
char  *strchr(const char *, int);
char  *strstr(const char *, const char *);
void  *memmove(void *, const void *, size_t);
void  *memcpy(void *, const void *, size_t);
int    memcmp(const void *, const void *, size_t);
void  *memchr(const void *, int, size_t);
void  *memccpy(void *, const void *, int, size_t);
void   bzero(void *, size_t);
void  *memset(void *, int, size_t);
void   memset_pattern4(void *, const void *, size_t);

int main(void)
{
    char buf[32];

    /* strlen / strnlen (the word-at-a-time portable Apple impl) */
    if (strlen("") != 0)            return 1;
    if (strlen("hello") != 5)       return 2;
    if (strlen("aligned8word!!") != 14) return 3;
    if (strnlen("hello", 3) != 3)   return 4;
    if (strnlen("hi", 9) != 2)      return 5;

    /* strcmp / strncmp */
    if (strcmp("abc", "abc") != 0)  return 6;
    if (strcmp("abc", "abd") >= 0)  return 7;
    if (strcmp("abd", "abc") <= 0)  return 8;
    if (strncmp("abcXX", "abcYY", 3) != 0) return 9;
    if (strncmp("abc", "abd", 3) >= 0)     return 10;

    /* strcpy / strncpy */
    strcpy(buf, "copy");
    if (strcmp(buf, "copy") != 0)   return 11;
    for (int i = 0; i < 8; i++) buf[i] = 'Z';
    strncpy(buf, "ab", 5);          /* pads with NUL to 5 */
    if (buf[0] != 'a' || buf[1] != 'b' || buf[2] || buf[3] || buf[4]) return 12;

    /* strlcpy / strlcat (BSD bounded, return intended length) */
    if (strlcpy(buf, "hello", sizeof buf) != 5 || strcmp(buf, "hello")) return 13;
    if (strlcat(buf, "!!", sizeof buf) != 7 || strcmp(buf, "hello!!"))  return 14;
    {
        char small[4];
        if (strlcpy(small, "toolong", sizeof small) != 7) return 15; /* truncates */
        if (strcmp(small, "too") != 0)                    return 16;
    }

    /* strchr / strstr */
    {
        const char *s = "path/to/x";
        if (strchr(s, '/') != s + 4)   return 17;
        if (strchr(s, 'Z') != NULL)    return 18;
        if (strchr(s, '\0') != s + 9)  return 19; /* finds the terminator */
        if (strstr(s, "to/") != s + 5) return 20;
        if (strstr(s, "nope") != NULL) return 21;
        if (strstr(s, "") != s)        return 22; /* empty needle -> start */
    }

    /* memmove (overlap, both directions) / memcpy */
    {
        char m[16];
        strcpy(m, "0123456789");
        memmove(m + 2, m, 5);          /* forward overlap */
        if (memcmp(m, "0101234789", 10) != 0) return 23;
        strcpy(m, "0123456789");
        memmove(m, m + 2, 5);          /* backward overlap */
        if (memcmp(m, "2345656789", 10) != 0) return 24;
        char dst[8];
        memcpy(dst, "abcdefg", 8);
        if (strcmp(dst, "abcdefg") != 0)      return 25;
    }

    /* memcmp / memchr / memccpy */
    if (memcmp("abcd", "abcd", 4) != 0)  return 26;
    if (memcmp("abcd", "abce", 4) >= 0)  return 27;
    {
        const char *s = "find_x_here";
        if (memchr(s, 'x', 11) != s + 5) return 28;
        if (memchr(s, 'Z', 11) != NULL)  return 29;
        char d[8];
        void *e = memccpy(d, "ab|cd", '|', sizeof d); /* copies through '|' */
        if (!e || (char *)e - d != 3 || d[0] != 'a' || d[1] != 'b' || d[2] != '|') return 30;
    }

    /* bzero / memset / memset_pattern4 (Apple's real memset goes through
       memset_pattern4 internally -- this is the genuine code path) */
    {
        char z[8]; for (int i = 0; i < 8; i++) z[i] = 0x55;
        bzero(z, 8);
        for (int i = 0; i < 8; i++) if (z[i]) return 31;
        memset(z, 0xAB, 8);
        for (int i = 0; i < 8; i++) if ((unsigned char)z[i] != 0xAB) return 32;
        char p[8];
        memset_pattern4(p, "WXYZ", 8);
        if (memcmp(p, "WXYZWXYZ", 8) != 0) return 33;
    }

    return 0;
}
