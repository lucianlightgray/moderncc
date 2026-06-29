/* Self-checking conformance for Apple's *genuine* self-contained formatter,
   _simple_snprintf / __simple_bprintf, compiled from Apple's verbatim
   libplatform source (src/simple/string_io.c -- see PROVENANCE.md) by mcc
   targeting x86_64 Mach-O, then loaded and executed as a Mach-O image by the
   in-repo loader on a Linux host.

   This is Apple's real formatted-output code from libSystem. Apple ships it
   precisely BECAUSE it has no FILE/locale/malloc dependency (low-level code
   like libmalloc's own error reporting uses it). The fixed-buffer entry
   `_simple_vsnprintf` writes into the caller's buffer via the real
   `__simple_bprintf` engine and never touches Mach VM -- the vm_allocate paths
   live only in the growing-string functions it does not call. So this exercises
   Apple's genuine printf engine (%d %i %u %o %x %X %p %c %s, width, zero-pad,
   length modifiers) as a Mach-O image, off-Darwin.

   Exits 0 on success; a nonzero return identifies the failing check. */

typedef unsigned long size_t;
int   _simple_snprintf(char *str, size_t size, const char *fmt, ...);
int   strcmp(const char *, const char *);   /* Apple's real strcmp (libplatform) */

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

    /* width + zero padding */
    _simple_snprintf(b, sizeof b, "[%5d][%05d]", 42, 42);
    if (!eq(b, "[   42][00042]")) return 5;

    /* long / long long length modifiers */
    _simple_snprintf(b, sizeof b, "ll=%lld lx=%lx", 3000000000LL, 0x1ffffffffUL);
    if (!eq(b, "ll=3000000000 lx=1ffffffff")) return 6;

    /* pointer */
    _simple_snprintf(b, sizeof b, "p=%p", (void *)0x1234);
    if (!eq(b, "p=0x1234")) return 7;

    /* return value = number of chars that would be written */
    if (_simple_snprintf(b, sizeof b, "abcd") != 4) return 8;

    /* bounded: truncates into the caller's buffer, never overflows */
    {
        char small[4];
        _simple_snprintf(small, sizeof small, "%d", 99999);
        if (small[3] != '\0') return 9;   /* always NUL-terminated */
    }

    return 0;
}
