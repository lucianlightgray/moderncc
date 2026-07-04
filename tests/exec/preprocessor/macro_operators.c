#include <stdio.h>
#include <string.h>

#define STR(x)      #x
#define XSTR(x)     STR(x)
#define CAT(a, b)   a ## b
#define VALUE       42

int main(void)
{

    printf("s1: [%s]\n", STR(hello));
    printf("s2: [%s]\n", STR( a   +   b ));
    printf("s3: [%s]\n", STR(1+2));

    printf("s4: [%s]\n", STR("quoted\n"));

    printf("noexpand: [%s]\n", STR(VALUE));
    printf("expand:   [%s]\n", XSTR(VALUE));

    int foobar = 7;
    printf("paste_id: %d\n", CAT(foo, bar));
    printf("paste_num: %d\n", CAT(1, 2) + CAT(3, 4));

    printf("empty_l: %d\n", CAT(, 99));
    printf("empty_r: %d\n", CAT(99, ));

    printf("paste_then_expand: %d\n", CAT(VAL, UE));
    return 0;
}
