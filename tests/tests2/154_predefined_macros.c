/* C99 6.10.8: predefined macros. Only assert values that are portable across
   gcc/clang/tcc and across checkout locations -- so __FILE__/__DATE__/__TIME__
   are probed for shape, not exact contents, and __LINE__ for relative deltas. */
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* __STDC__ is 1; __STDC_VERSION__ is 199901L for C99 (also in gnu99) */
    printf("stdc: %d\n", __STDC__);
    printf("version: %ld\n", (long)__STDC_VERSION__);

    /* __LINE__ tracks the physical line; consecutive uses differ by one */
    int a = __LINE__;
    int b = __LINE__;
    printf("line_delta: %d\n", b - a);

    /* __LINE__ expands at the point of use, even inside an expression */
    int here = __LINE__; printf("same_line: %d\n", here == __LINE__);

    /* __FILE__ is this translation unit's name (path varies, so just probe it) */
    printf("file_ok: %d\n", strstr(__FILE__, "154_predefined_macros.c") != NULL);

    /* __DATE__ is "Mmm dd yyyy" (11 chars), __TIME__ is "hh:mm:ss" (8 chars) */
    printf("date_len: %d time_len: %d\n",
           (int)strlen(__DATE__), (int)strlen(__TIME__));
    return 0;
}
