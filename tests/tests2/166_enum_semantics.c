/* C99 6.7.2.2: enumeration specifiers. Enumerators have type int; an
   unspecified enumerator is one more than the previous (the first defaults to
   0); explicit values may repeat, be negative, or be given out of order; a
   trailing comma in the enumerator list is permitted; and the enumerated type
   is usable in switch/arithmetic contexts. */
#include <stdio.h>

/* auto-increment from 0, with a trailing comma (legal in C99) */
enum color { RED, GREEN, BLUE, };

/* explicit, negative, repeated, and resumed auto-increment values */
enum mixed {
    NEG = -3,
    ZERO = 0,
    ONE,            /* ZERO + 1 == 1 */
    TWO,            /* 2 */
    ALSO_TWO = 2,   /* duplicate value, allowed */
    TEN = 10,
    ELEVEN          /* TEN + 1 == 11 */
};

int main(void)
{
    printf("auto: %d %d %d\n", RED, GREEN, BLUE);       /* 0 1 2 */
    printf("mixed: %d %d %d %d\n", NEG, ONE, TWO, ELEVEN);

    /* duplicate enumerators compare equal */
    printf("dup: %d\n", TWO == ALSO_TWO);               /* 1 */

    /* enumerators are integer constant expressions: usable in array sizes,
       case labels, and arithmetic */
    int sizes[BLUE + 1];
    printf("count: %d\n", (int)(sizeof sizes / sizeof sizes[0]));   /* 3 */

    enum color c = GREEN;
    switch (c) {
    case RED:   printf("sw: red\n");   break;
    case GREEN: printf("sw: green\n"); break;
    case BLUE:  printf("sw: blue\n");  break;
    }

    /* enumerators have type int (compatible with int arithmetic) */
    printf("arith: %d\n", TEN * 2 + ONE);               /* 21 */
    return 0;
}
