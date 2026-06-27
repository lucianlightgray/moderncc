/* C99 6.10.4: the #line directive resets the line number (and optionally the
   file name) that __LINE__ / __FILE__ report for the following lines. */
#include <stdio.h>
#include <string.h>

int main(void)
{
#line 100
    int l100 = __LINE__;          /* this line is now 100 */
    int l101 = __LINE__;          /* 101 */
    printf("set100: %d %d\n", l100, l101);

#line 1
    int back = __LINE__;          /* renumbered back to 1 */
    printf("reset: %d\n", back);

#line 42 "synthetic.c"
    /* tcc path-normalizes the #line filename (it may prepend the source dir),
       so match the trailing component rather than the whole string -- that is
       deterministic across gcc/clang/tcc. */
    printf("withfile: line=%d file_match=%d\n",
           __LINE__, strstr(__FILE__, "synthetic.c") != NULL);

    /* line numbering keeps incrementing from the directive's value */
    printf("next: %d\n", __LINE__);
    return 0;
}
