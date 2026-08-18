/* T-mac-30153: __FILE_NAME__ (basename of the current file) and __TIMESTAMP__
 * (mtime, ctime format) must be predefined like gcc/clang. Exit 0 iff correct. */
#include <string.h>
int main(void){
#ifndef __FILE_NAME__
    return 1;
#endif
#ifndef __TIMESTAMP__
    return 2;
#endif
    if (strcmp(__FILE_NAME__, "loc_macros.c") != 0) return 3;
    /* __TIMESTAMP__ is a non-empty string literal of the ctime shape (len 24) */
    if (sizeof(__TIMESTAMP__) != 25) return 4;
    return 0;
}
