




#include <stdio.h>


enum color { RED, GREEN, BLUE, };


enum mixed {
    NEG = -3,
    ZERO = 0,
    ONE,
    TWO,
    ALSO_TWO = 2,
    TEN = 10,
    ELEVEN
};

int main(void)
{
    printf("auto: %d %d %d\n", RED, GREEN, BLUE);
    printf("mixed: %d %d %d %d\n", NEG, ONE, TWO, ELEVEN);


    printf("dup: %d\n", TWO == ALSO_TWO);



    int sizes[BLUE + 1];
    printf("count: %d\n", (int)(sizeof sizes / sizeof sizes[0]));

    enum color c = GREEN;
    switch (c) {
    case RED:   printf("sw: red\n");   break;
    case GREEN: printf("sw: green\n"); break;
    case BLUE:  printf("sw: blue\n");  break;
    }


    printf("arith: %d\n", TEN * 2 + ONE);
    return 0;
}
