// permutation: single-line comment at the very start of the file
/* permutation: block comment at the very start of the file,
   spanning
   several
   lines (block comment across many lines) */
#include <stdio.h>          // permutation: trailing line comment after a directive

#define A /* leading block comment inside a macro body */ 1
#define B 2 /* trailing block comment inside a macro body */
#define C 3                 // trailing line comment inside a macro body

int main(void) /* block comment at the end of a line */
{
    /* permutation: block comment on its own line (start of line, single line) */
    int x = A /* permutation: block comment in the middle of a statement */ + B + C;
    printf("x=%d\n", x);     // expect 6

    /* permutation: multi-line block comment
       sitting
       across many lines
       in the middle of the function body */
    int y = 1 +/**/2;        // permutation: empty block comment used as a token separator
    printf("y=%d\n", y);     // expect 3

    int z = 10 - /* block comment keeps the two minus signs apart */ -3;
    printf("z=%d\n", z);     // expect 13

    // permutation: comment containing comment-like and string-like text: /* "x */ //
    char *s = "/* not a comment */ // also not"; /* the string literal is untouched */
    printf("%s\n", s);

    /* permutation: C comments do not nest, a /* fake opener is just text */
    int w = 7;
    printf("w=%d\n", w);     // expect 7

    int q = 4;               // permutation: line comment continued by a backslash \
    q = 999;                 // this whole line is spliced into the comment above, so it never runs
    printf("q=%d\n", q);     // expect 4

    char slash = '/';        /* permutation: char literal '/', not the start of a comment */
    char star  = '*';        // permutation: char literal '*', not the end of a comment
    printf("%c%c\n", slash, star);   // expect /*

    return 0;
}
// permutation: single-line comment at the end of the file
/* permutation: block comment
   at the end of the file */
