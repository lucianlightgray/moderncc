/* 5.1.1.2 phase 1: trigraph replacement (enabled with -trigraphs).
   Exercises all nine sequences. Built with the "-trigraphs" flag (goldens row).
   ??=  #     ??(  [     ??)  ]     ??<  {     ??>  }
   ??/  \     ??'  ^     ??!  |     ??-  ~                             */
??=include <stddef.h>            /* ??= -> #  (directive)             */

extern int printf(const char *, ...);

int main(void)
??<                              /* ??< -> {                          */
    int a??(3??);                /* ??( ??) -> [ ]                    */
    a??(0??) = 6 ??! 1;          /* ??! -> |   : 6|1 == 7             */
    a??(1??) = 5 ??' 3;          /* ??' -> ^   : 5^3 == 6             */
    a??(2??) = ??-0;             /* ??- -> ~   : ~0 == -1             */

    int x = 1 ??/
            + 9;                 /* ??/ -> \   : line splice -> 10    */

    int ok = a??(0??) == 7 && a??(1??) == 6 && a??(2??) == -1 && x == 10;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
??>                              /* ??> -> }                          */
