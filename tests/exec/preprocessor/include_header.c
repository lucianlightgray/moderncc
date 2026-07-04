#include <stdio.h>
#include "inc_header.h"
#include "inc_header.h"

#ifndef INC188_H
#error "header guard macro not defined after include"
#endif

int main(void) {

    printf("answer: %d\n", INC188_ANSWER);

    printf("triple: %d\n", inc188_triple(14));

    printf("combined: %d\n", inc188_triple(INC188_ANSWER) - 84);
    return 0;
}
